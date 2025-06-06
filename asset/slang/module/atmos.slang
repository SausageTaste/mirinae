module atmos;

static const float PI = 3.14159265358979323846;

public static const float AP_KM_PER_SLICE = 4;
public static const float AP_SLICE_COUNT = 32;
public static const float AP_SLICE_COUNT_RCP = 1.0 / AP_SLICE_COUNT;
public static const float M_TO_KM = 0.001;
public static const float PLANET_RADIUS_OFFSET = 0.01;
public static const int MULTI_SCAT_LUT_RES = 32;


public struct AtmosphereParameters {
    // Radius of the planet (center to ground)
    float BottomRadius;
    // Maximum considered atmosphere height (center to atmosphere top)
    float TopRadius;

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


float fromUnitToSubUvs(float u, float resolution) {
    return (u + 0.5 / resolution) * (resolution / (resolution + 1.0));
}


float fromSubUvsToUnit(float u, float resolution) {
    return (u - 0.5 / resolution) * (resolution / (resolution - 1.0));
}


public float AerialPerspectiveDepthToSlice(float depth) {
    const float AP_KM_PER_SLICE = 4;
    const float M_PER_SLICE_RCP = 1.0 / (AP_KM_PER_SLICE * 1000.0);
    return depth * M_PER_SLICE_RCP;
}


float3 GetMultipleScattering(
    AtmosphereParameters Atmosphere,
    Sampler2D multi_scat,
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

    float3 multiScatteredLuminance = multi_scat.SampleLevel(uv, 0).rgb;
    return multiScatteredLuminance;
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


bool MoveToTopAtmosphere(inout float3 WorldPos, in float3 WorldDir, in float AtmosphereTopRadius) {
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


struct SingleScatteringResult {
    float3 L;              // Scattered light (luminance)
    float3 OpticalDepth;   // Optical depth (1/m)
    float3 Transmittance;  // Transmittance in [0,1] (unitless)
    float3 MultiScatAs1;

    float3 NewMultiScatStep0Out;
    float3 NewMultiScatStep1Out;
};


public float3 get_transmittance(
    float3 frag_pos_w,
    float3 sun_dir_w,
    Sampler2D trans_lut,
    const AtmosphereParameters atmos_params
) {
    const float planet_radius = atmos_params.BottomRadius * 1000;
    const float3 frag_pos_e = frag_pos_w + float3(0, planet_radius, 0);
    const float frag_height_e = length(frag_pos_e);
    const float3 frag_up_dir_e = normalize(frag_pos_e);
    const float view_zenith_cos_angle = dot(sun_dir_w, frag_up_dir_e);
    const float2 lut_trans_uv = LutTransmittanceParamsToUv(
        atmos_params, frag_height_e / 1000.0, view_zenith_cos_angle
    );
    return trans_lut.SampleLevel(lut_trans_uv, 0).xyz;
}
