module atmos;

static const float AP_KM_PER_SLICE = 4;
static const float AP_SLICE_COUNT = 32;
static const float M_TO_KM = 0.001;
static const float PI = 3.14159265358979323846;
static const int MULTI_SCAT_LUT_RES = 32;

public static const float PLANET_RADIUS_OFFSET = 0.01;
public static const float AP_SLICE_COUNT_RCP = 1.0 / AP_SLICE_COUNT;


public struct AtmosphereParameters {
    // Radius of the planet (center to ground)
    public float BottomRadius;
    // Maximum considered atmosphere height (center to atmosphere top)
    public float TopRadius;

    // Rayleigh scattering exponential distribution scale in the atmosphere
    float RayleighDensityExpScale;
    // Rayleigh scattering coefficients
    float3 RayleighScattering;

    // Mie scattering exponential distribution scale in the atmosphere
    float MieDensityExpScale;
    // Mie scattering coefficients
    float3 MieScattering;
    // Mie extinction coefficients
    float3 MieExtinction;
    // Mie absorption coefficients
    float3 MieAbsorption;
    // Mie phase function excentricity
    float MiePhaseG;

    // Another medium type in the atmosphere
    float AbsorptionDensity0LayerWidth;
    float AbsorptionDensity0ConstantTerm;
    float AbsorptionDensity0LinearTerm;
    float AbsorptionDensity1ConstantTerm;
    float AbsorptionDensity1LinearTerm;
    // This other medium only absorb light, e.g. useful to represent ozone in the earth atmosphere
    float3 AbsorptionExtinction;

    // The albedo of the ground.
    float3 GroundAlbedo;
};


public AtmosphereParameters GetAtmosphereParameters() {
    AtmosphereParameters Parameters;
    Parameters.AbsorptionExtinction = float3(0.00065, 0.00188, 0.00008);

    // Traslation from Bruneton2017 parameterisation.
    Parameters.RayleighDensityExpScale = -0.125;
    Parameters.MieDensityExpScale = -0.83333;
    Parameters.AbsorptionDensity0LayerWidth = 25;
    Parameters.AbsorptionDensity0ConstantTerm = -0.66667;
    Parameters.AbsorptionDensity0LinearTerm = 0.06667;
    Parameters.AbsorptionDensity1ConstantTerm = 2.66667;
    Parameters.AbsorptionDensity1LinearTerm = -0.06667;

    Parameters.MiePhaseG = 0.8;
    Parameters.RayleighScattering = float3(0.0058, 0.01356, 0.0331);
    Parameters.MieScattering = float3(0.004);
    Parameters.MieAbsorption = float3(0.00044);
    Parameters.MieExtinction = float3(0.00444);
    Parameters.GroundAlbedo = float3(0);
    Parameters.BottomRadius = 6360;
    Parameters.TopRadius = 6460;
    return Parameters;
}


float getAlbedo(float scattering, float extinction) {
    return scattering / max(0.001, extinction);
}


float3 getAlbedo(float3 scattering, float3 extinction) {
    return scattering / max(0.001, extinction);
}


float RayleighPhase(float cosTheta) {
    float factor = 3.0f / (16.0f * PI);
    return factor * (1.0f + cosTheta * cosTheta);
}


float CornetteShanksMiePhaseFunction(float g, float cosTheta) {
    float k = 3.0 / (8.0 * PI) * (1.0 - g * g) / (2.0 + g * g);
    return k * (1.0 + cosTheta * cosTheta) / pow(1.0 + g * g - 2.0 * g * -cosTheta, 1.5);
}


float hgPhase(float g, float cosTheta) {
    return CornetteShanksMiePhaseFunction(g, cosTheta);
}


public void UvToLutTransmittanceParams(
    AtmosphereParameters Atmosphere,
    out float viewHeight,
    out float viewZenithCosAngle,
    in float2 uv
) {
    // uv = float2(fromSubUvsToUnit(uv.x, TRANSMITTANCE_TEXTURE_WIDTH), fromSubUvsToUnit(uv.y,
    // TRANSMITTANCE_TEXTURE_HEIGHT)); // No real impact so off
    float x_mu = uv.x;
    float x_r = uv.y;

    float H = sqrt(
        Atmosphere.TopRadius * Atmosphere.TopRadius -
        Atmosphere.BottomRadius * Atmosphere.BottomRadius
    );
    float rho = H * x_r;
    viewHeight = sqrt(rho * rho + Atmosphere.BottomRadius * Atmosphere.BottomRadius);

    float d_min = Atmosphere.TopRadius - viewHeight;
    float d_max = rho + H;
    float d = d_min + x_mu * (d_max - d_min);
    viewZenithCosAngle = d == 0.0 ? 1.0f : (H * H - rho * rho - d * d) / (2.0 * viewHeight * d);
    viewZenithCosAngle = clamp(viewZenithCosAngle, -1.0, 1.0);
}


float2 LutTransmittanceParamsToUv(
    AtmosphereParameters Atmosphere, float viewHeight, float viewZenithCosAngle
) {
    float H = sqrt(
        max(0.0f,
            Atmosphere.TopRadius * Atmosphere.TopRadius -
                Atmosphere.BottomRadius * Atmosphere.BottomRadius)
    );
    float rho = sqrt(
        max(0.0f, viewHeight * viewHeight - Atmosphere.BottomRadius * Atmosphere.BottomRadius)
    );

    float discriminant = viewHeight * viewHeight * (viewZenithCosAngle * viewZenithCosAngle - 1.0) +
                         Atmosphere.TopRadius * Atmosphere.TopRadius;
    float d = max(
        0.0, (-viewHeight * viewZenithCosAngle + sqrt(discriminant))
    );  // Distance to atmosphere boundary

    float d_min = Atmosphere.TopRadius - viewHeight;
    float d_max = rho + H;
    float x_mu = (d - d_min) / (d_max - d_min);
    float x_r = rho / H;

    return float2(x_mu, x_r);
    // uv = float2(fromUnitToSubUvs(uv.x, TRANSMITTANCE_TEXTURE_WIDTH), fromUnitToSubUvs(uv.y,
    // TRANSMITTANCE_TEXTURE_HEIGHT)); // No real impact so off
}


public void UvToSkyViewLutParams<let TNonLinearSkyViewLut : bool>(
    AtmosphereParameters Atmosphere,
    out float viewZenithCosAngle,
    out float lightViewCosAngle,
    in float viewHeight,
    in float2 uv
) {
    // Constrain uvs to valid sub texel range (avoid zenith derivative issue making LUT usage
    // visible)
    uv = float2(fromSubUvsToUnit(uv.x, 192.0f), fromSubUvsToUnit(uv.y, 108.0f));

    float Vhorizon = sqrt(
        viewHeight * viewHeight - Atmosphere.BottomRadius * Atmosphere.BottomRadius
    );
    float CosBeta = Vhorizon / viewHeight;  // GroundToHorizonCos
    float Beta = acos(CosBeta);
    float ZenithHorizonAngle = PI - Beta;

    if (uv.y < 0.5f) {
        float coord = 2.0 * uv.y;
        coord = 1.0 - coord;
        if (TNonLinearSkyViewLut)
            coord *= coord;
        coord = 1.0 - coord;
        viewZenithCosAngle = cos(ZenithHorizonAngle * coord);
    } else {
        float coord = uv.y * 2.0 - 1.0;
        if (TNonLinearSkyViewLut)
            coord *= coord;
        viewZenithCosAngle = cos(ZenithHorizonAngle + Beta * coord);
    }

    float coord = uv.x;
    coord *= coord;
    lightViewCosAngle = -(coord * 2.0 - 1.0);
}


float fromUnitToSubUvs(float u, float resolution) {
    return (u + 0.5 / resolution) * (resolution / (resolution + 1.0));
}


public float fromSubUvsToUnit(float u, float resolution) {
    return (u - 0.5 / resolution) * (resolution / (resolution - 1.0));
}


public float AerialPerspectiveDepthToSlice(float depth) {
    const float AP_KM_PER_SLICE = 4;
    const float M_PER_SLICE_RCP = 1.0 / (AP_KM_PER_SLICE * 1000.0);
    return depth * M_PER_SLICE_RCP;
}


// - r0: ray origin
// - rd: normalized ray direction
// - s0: sphere center
// - sR: sphere radius
// - Returns distance from r0 to first intersecion with sphere,
//   or -1.0 if no intersection.
float raySphereIntersectNearest(float3 r0, float3 rd, float3 s0, float sR) {
    float a = dot(rd, rd);
    float3 s0_r0 = r0 - s0;
    float b = 2.0 * dot(rd, s0_r0);
    float c = dot(s0_r0, s0_r0) - (sR * sR);
    float delta = b * b - 4.0 * a * c;
    if (delta < 0.0 || a == 0.0) {
        return -1.0;
    }
    float sol0 = (-b - sqrt(delta)) / (2.0 * a);
    float sol1 = (-b + sqrt(delta)) / (2.0 * a);
    if (sol0 < 0.0 && sol1 < 0.0) {
        return -1.0;
    }
    if (sol0 < 0.0) {
        return max(0.0, sol1);
    } else if (sol1 < 0.0) {
        return max(0.0, sol0);
    }
    return max(0.0, min(sol0, sol1));
}


public bool MoveToTopAtmosphere(
    inout float3 WorldPos, in float3 WorldDir, in float AtmosphereTopRadius
) {
    float viewHeight = length(WorldPos);
    if (viewHeight > AtmosphereTopRadius) {
        float tTop = raySphereIntersectNearest(WorldPos, WorldDir, float3(0), AtmosphereTopRadius);
        if (tTop >= 0) {
            float3 UpVector = WorldPos / viewHeight;
            float3 UpOffset = UpVector * -PLANET_RADIUS_OFFSET;
            WorldPos = WorldPos + WorldDir * tTop + UpOffset;
        } else {
            // Ray is not intersecting the atmosphere
            return false;
        }
    }
    return true;  // ok to start tracing
}


struct MediumSampleRGB {
    float3 scattering;
    float3 absorption;
    float3 extinction;

    float3 scatteringMie;
    float3 absorptionMie;
    float3 extinctionMie;

    float3 scatteringRay;
    float3 absorptionRay;
    float3 extinctionRay;

    float3 scatteringOzo;
    float3 absorptionOzo;
    float3 extinctionOzo;

    float3 albedo;
};


MediumSampleRGB sampleMediumRGB(float3 WorldPos, AtmosphereParameters Atmosphere) {
    const float viewHeight = length(WorldPos) - Atmosphere.BottomRadius;

    const float densityMie = exp(Atmosphere.MieDensityExpScale * viewHeight);
    const float densityRay = exp(Atmosphere.RayleighDensityExpScale * viewHeight);
    const float densityOzo = saturate(
        viewHeight < Atmosphere.AbsorptionDensity0LayerWidth
            ? Atmosphere.AbsorptionDensity0LinearTerm * viewHeight +
                  Atmosphere.AbsorptionDensity0ConstantTerm
            : Atmosphere.AbsorptionDensity1LinearTerm * viewHeight +
                  Atmosphere.AbsorptionDensity1ConstantTerm
    );

    MediumSampleRGB s;

    s.scatteringMie = densityMie * Atmosphere.MieScattering;
    s.absorptionMie = densityMie * Atmosphere.MieAbsorption;
    s.extinctionMie = densityMie * Atmosphere.MieExtinction;

    s.scatteringRay = densityRay * Atmosphere.RayleighScattering;
    s.absorptionRay = float3(0);
    s.extinctionRay = s.scatteringRay + s.absorptionRay;

    s.scatteringOzo = float3(0);
    s.absorptionOzo = densityOzo * Atmosphere.AbsorptionExtinction;
    s.extinctionOzo = s.scatteringOzo + s.absorptionOzo;

    s.scattering = s.scatteringMie + s.scatteringRay + s.scatteringOzo;
    s.absorption = s.absorptionMie + s.absorptionRay + s.absorptionOzo;
    s.extinction = s.extinctionMie + s.extinctionRay + s.extinctionOzo;
    s.albedo = getAlbedo(s.scattering, s.extinction);

    return s;
}


public struct SingleScatteringResult {
    public float3 L;               // Scattered light (luminance)
    public float3 optical_depth_;  // Optical depth (1/m)
    float3 transmittance_;         // Transmittance in [0,1] (unitless)
    public float3 MultiScatAs1;

    float3 NewMultiScatStep0Out;
    float3 NewMultiScatStep1Out;
};


public interface ITransLut {
    float3 sample(AtmosphereParameters atmos_params, float height, float cos_angle);
}


public struct NullTransLut : ITransLut {
    public float3 sample(AtmosphereParameters atmos_params, float height, float cos_angle) {
        return float3(1);  // No transmittance
    }
};


public struct SamplerTransLut : ITransLut {
    public __init(Sampler2D tex) {
        this.tex_ = tex;
    }

    public float3 sample(AtmosphereParameters atmos_params, float height, float cos_angle) {
        let uv = LutTransmittanceParamsToUv(atmos_params, height, cos_angle);
        return tex_.SampleLevel(uv, 0).xyz;
    }

    private Sampler2D tex_;
};


public interface IMulScatLut {
    float3 sample(
        AtmosphereParameters Atmosphere,
        float3 scattering,
        float3 extinction,
        float3 worlPos,
        float viewZenithCosAngle
    );
}


public struct NullMulScatLut : IMulScatLut {
    public float3 sample(
        AtmosphereParameters Atmosphere,
        float3 scattering,
        float3 extinction,
        float3 worlPos,
        float viewZenithCosAngle
    ) {
        return float3(0);  // No multi scattering
    }
};


public struct SamplerMulScatLut : IMulScatLut {
    public __init(Sampler2D tex) {
        this.tex_ = tex;
    }

    public float3 sample(
        AtmosphereParameters Atmosphere,
        float3 scattering,
        float3 extinction,
        float3 worlPos,
        float viewZenithCosAngle
    ) {
        float2 uv = saturate(float2(
            viewZenithCosAngle * 0.5f + 0.5f,
            (length(worlPos) - Atmosphere.BottomRadius) /
                (Atmosphere.TopRadius - Atmosphere.BottomRadius)
        ));
        uv = float2(
            fromUnitToSubUvs(uv.x, MULTI_SCAT_LUT_RES), fromUnitToSubUvs(uv.y, MULTI_SCAT_LUT_RES)
        );

        return tex_.SampleLevel(uv, 0).xyz;
    }

    private Sampler2D tex_;
};


public float3 get_transmittance(
    const float3 frag_pos_w,
    const float3 sun_dir_w,
    const ITransLut trans_lut,
    const AtmosphereParameters atmos_params
) {
    const float planet_radius = atmos_params.BottomRadius * 1000;
    const float3 frag_pos_e = frag_pos_w + float3(0, planet_radius, 0);
    const float frag_height_e = length(frag_pos_e);
    const float3 frag_up_dir_e = normalize(frag_pos_e);
    const float view_zenith_cos_angle = dot(sun_dir_w, frag_up_dir_e);
    return trans_lut.sample(atmos_params, frag_height_e * M_TO_KM, view_zenith_cos_angle);
}


// Scattered luminance integration parameters
public struct ScatLumiIntParams {
    public __init() {
        gSkyInvViewProjMat = float4x4(1);
        gSunIlluminance = float3(1);
        WorldPos = float3(0, 0, 0);
        WorldDir = float3(0, 1, 0);
        SunDir = float3(0, 1, 0);
        pixPos = float2(0, 0);
        gResolution = float2(10, 10);
        RayMarchMinMaxSPP = float2(4, 128);
        SampleCountIni = 10;
        DepthBufferValue = -1;
        tMaxMax = 9000000;
        VariableSampleCount = false;
        MieRayPhase = false;
        ground = false;
    }

    public float4x4 gSkyInvViewProjMat;

    // When building the scattering factor, we assume light illuminance is 1 to compute a transfert
    // function relative to identity illuminance of 1. This make the scattering factor independent
    // of the light.It is now only linked to the atmosphere properties.
    public float3 gSunIlluminance;

    public float3 WorldPos;
    public float3 WorldDir;
    public float3 SunDir;

    public float2 pixPos;
    public float2 gResolution;
    public float2 RayMarchMinMaxSPP;

    public float SampleCountIni;
    public float DepthBufferValue;
    public float tMaxMax;
    public bool VariableSampleCount;
    public bool MieRayPhase;
    public bool ground;
};


public SingleScatteringResult IntegrateScatteredLuminance(
    const ITransLut trans_lut,
    const IMulScatLut multi_scat,
    const AtmosphereParameters Atmosphere,
    const ScatLumiIntParams params
) {
    SingleScatteringResult result;
    result.MultiScatAs1 = 0;

    float3 ClipSpace = float3((params.pixPos / params.gResolution) * 2 - 1, 1);

    // Compute next intersection with atmosphere or ground
    float3 earthO = float3(0.0f, 0.0f, 0.0f);
    float tBottom = raySphereIntersectNearest(
        params.WorldPos, params.WorldDir, earthO, Atmosphere.BottomRadius
    );
    float tTop = raySphereIntersectNearest(
        params.WorldPos, params.WorldDir, earthO, Atmosphere.TopRadius
    );
    float tMax = 0.0f;
    if (tBottom < 0.0f) {
        if (tTop < 0.0f) {
            tMax = 0.0f;  // No intersection with earth nor atmosphere: stop right away
            return result;
        } else {
            tMax = tTop;
        }
    } else {
        if (tTop > 0.0f) {
            tMax = min(tTop, tBottom);
        }
    }

    if (params.DepthBufferValue >= 0.0f) {
        ClipSpace.z = params.DepthBufferValue;
        if (ClipSpace.z < 1.0f) {
            float4 DepthBufferWorldPos = mul(params.gSkyInvViewProjMat, float4(ClipSpace, 1.0));
            DepthBufferWorldPos /= DepthBufferWorldPos.w;

            // apply earth offset to go back to origin as top of earth mode.
            float tDepth = length(
                DepthBufferWorldPos.xyz - (params.WorldPos + float3(0, -Atmosphere.BottomRadius, 0))
            );
            if (tDepth < tMax) {
                tMax = tDepth;
            }
        }
    }
    tMax = min(tMax, params.tMaxMax);

    // Sample count
    float SampleCount = params.SampleCountIni;
    float SampleCountFloor = params.SampleCountIni;
    float tMaxFloor = tMax;
    if (params.VariableSampleCount) {
        SampleCount = lerp(
            params.RayMarchMinMaxSPP.x, params.RayMarchMinMaxSPP.y, saturate(tMax * 0.01)
        );
        SampleCountFloor = floor(SampleCount);
        // rescale tMax to map to the last entire step segment.
        tMaxFloor = tMax * SampleCountFloor / SampleCount;
    }
    float dt = tMax / SampleCount;

    // Phase functions
    const float uniformPhase = 1.0 / (4.0 * PI);
    const float3 wi = params.SunDir;
    const float3 wo = params.WorldDir;
    float cosTheta = dot(wi, wo);
    // mnegate cosTheta because due to WorldDir being a "in" direction.
    float MiePhaseValue = hgPhase(Atmosphere.MiePhaseG, -cosTheta);
    float RayleighPhaseValue = RayleighPhase(cosTheta);

    // Ray march the atmosphere to integrate optical depth
    float3 L = 0.0f;
    float3 throughput = 1.0;
    float3 OpticalDepth = 0.0;
    float t = 0.0f;
    float tPrev = 0.0;
    const float SampleSegmentT = 0.3f;
    for (float s = 0.0f; s < SampleCount; s += 1.0f) {
        if (params.VariableSampleCount) {
            // More expenssive but artefact free
            float t0 = (s) / SampleCountFloor;
            float t1 = (s + 1.0f) / SampleCountFloor;
            // Non linear distribution of sample within the range.
            t0 = t0 * t0;
            t1 = t1 * t1;
            // Make t0 and t1 world space distances.
            t0 = tMaxFloor * t0;
            if (t1 > 1.0) {
                t1 = tMax;
                //	t1 = tMaxFloor;	// this reveal depth slices
            } else {
                t1 = tMaxFloor * t1;
            }
            // t = t0 + (t1 - t0) * (whangHashNoise(pixPos.x, pixPos.y, gFrameId * 1920 * 1080)); //
            // With dithering required to hide some sampling artefact relying on TAA later? This may
            // even allow volumetric shadow?
            t = t0 + (t1 - t0) * SampleSegmentT;
            dt = t1 - t0;
        } else {
            // t = tMax * (s + SampleSegmentT) / SampleCount;
            //  Exact difference, important for accuracy of multiple scattering
            float NewT = tMax * (s + SampleSegmentT) / SampleCount;
            dt = NewT - t;
            t = NewT;
        }
        float3 P = params.WorldPos + t * params.WorldDir;

        MediumSampleRGB medium = sampleMediumRGB(P, Atmosphere);
        const float3 SampleOpticalDepth = medium.extinction * dt;
        const float3 SampleTransmittance = exp(-SampleOpticalDepth);
        OpticalDepth += SampleOpticalDepth;

        const float pHeight = length(P);
        const float3 UpVector = P / pHeight;
        const float SunZenithCosAngle = dot(params.SunDir, UpVector);
        let TransmittanceToSun = trans_lut.sample(Atmosphere, pHeight, SunZenithCosAngle);

        float3 PhaseTimesScattering;
        if (params.MieRayPhase) {
            PhaseTimesScattering = medium.scatteringMie * MiePhaseValue +
                                   medium.scatteringRay * RayleighPhaseValue;
        } else {
            PhaseTimesScattering = medium.scattering * uniformPhase;
        }

        // Earth shadow
        float tEarth = raySphereIntersectNearest(
            P, params.SunDir, earthO + PLANET_RADIUS_OFFSET * UpVector, Atmosphere.BottomRadius
        );
        float earthShadow = tEarth >= 0.0f ? 0.0f : 1.0f;

        // Dual scattering for multi scattering

        let multiScatteredLuminance = multi_scat.sample(
            Atmosphere, medium.scattering, medium.extinction, P, SunZenithCosAngle
        );

        float shadow = 1.0f;
        // First evaluate opaque shadow
        // shadow = getShadow(Atmosphere, P);

        float3 S = params.gSunIlluminance *
                   (earthShadow * shadow * TransmittanceToSun * PhaseTimesScattering +
                    multiScatteredLuminance * medium.scattering);
        float3 MS = medium.scattering * 1;
        float3 MSint = (MS - MS * SampleTransmittance) / medium.extinction;
        result.MultiScatAs1 += throughput * MSint;

        // Evaluate input to multi scattering
        {
            float3 newMS;

            newMS = earthShadow * TransmittanceToSun * medium.scattering * uniformPhase * 1;
            result.NewMultiScatStep0Out += throughput * (newMS - newMS * SampleTransmittance) /
                                           medium.extinction;
            //	result.NewMultiScatStep0Out += SampleTransmittance * throughput * newMS * dt;

            newMS = medium.scattering * uniformPhase * multiScatteredLuminance;
            result.NewMultiScatStep1Out += throughput * (newMS - newMS * SampleTransmittance) /
                                           medium.extinction;
            //	result.NewMultiScatStep1Out += SampleTransmittance * throughput * newMS * dt;
        }

        // See slide 28 at
        // http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/

        // integrate along the current step segment
        float3 Sint = (S - S * SampleTransmittance) / medium.extinction;
        // accumulate and also take into account the transmittance from previous steps
        L += throughput * Sint;
        throughput *= SampleTransmittance;
        tPrev = t;
    }

    if (params.ground && tMax == tBottom && tBottom > 0.0) {
        // Account for bounced light off the earth
        float3 P = params.WorldPos + tBottom * params.WorldDir;
        float pHeight = length(P);

        const float3 UpVector = P / pHeight;
        float SunZenithCosAngle = dot(params.SunDir, UpVector);
        let TransmittanceToSun = trans_lut.sample(Atmosphere, pHeight, SunZenithCosAngle);

        const float NdotL = saturate(dot(normalize(UpVector), normalize(params.SunDir)));
        L += params.gSunIlluminance * TransmittanceToSun * throughput * NdotL *
             Atmosphere.GroundAlbedo / PI;
    }

    result.L = L;
    result.optical_depth_ = OpticalDepth;
    result.transmittance_ = throughput;
    return result;
}
