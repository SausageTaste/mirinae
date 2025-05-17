
const float PLANET_RADIUS_OFFSET = 0.01;


float saturate(float x) {
    return clamp(x, 0.0, 1.0);
}

vec2 saturate(vec2 x) {
    return clamp(x, vec2(0.0), vec2(1.0));
}

vec3 getAlbedo(vec3 scattering, vec3 extinction) {
    return scattering / max(vec3(0.001), extinction);
}

float RayleighPhase(float cosTheta) {
    float factor = 3.0f / (16.0 * PI);
    return factor * (1.0 + cosTheta * cosTheta);
}

float CornetteShanksMiePhaseFunction(float g, float cosTheta) {
    float k = 3.0 / (8.0 * PI) * (1.0 - g * g) / (2.0 + g * g);
    return k * (1.0 + cosTheta * cosTheta) / pow(1.0 + g * g - 2.0 * g * -cosTheta, 1.5);
}

float hgPhase(float g, float cosTheta) {
    return CornetteShanksMiePhaseFunction(g, cosTheta);
}

vec2 LutTransmittanceParamsToUv(AtmosphereParameters Atmosphere, float viewHeight, float viewZenithCosAngle) {
    float H = sqrt(max(0.0f, Atmosphere.TopRadius * Atmosphere.TopRadius - Atmosphere.BottomRadius * Atmosphere.BottomRadius));
    float rho = sqrt(max(0.0f, viewHeight * viewHeight - Atmosphere.BottomRadius * Atmosphere.BottomRadius));

    float discriminant = viewHeight * viewHeight * (viewZenithCosAngle * viewZenithCosAngle - 1.0) + Atmosphere.TopRadius * Atmosphere.TopRadius;
    float d = max(0.0, (-viewHeight * viewZenithCosAngle + sqrt(discriminant))); // Distance to atmosphere boundary

    float d_min = Atmosphere.TopRadius - viewHeight;
    float d_max = rho + H;
    float x_mu = (d - d_min) / (d_max - d_min);
    float x_r = rho / H;

    return vec2(x_mu, x_r);
}

float fromUnitToSubUvs(float u, float resolution) { return (u + 0.5 / resolution) * (resolution / (resolution + 1.0)); }

vec3 GetMultipleScattering(AtmosphereParameters Atmosphere, sampler2D multi_scat, vec3 scattering, vec3 extinction, vec3 worlPos, float viewZenithCosAngle) {
    vec2 uv = saturate(vec2(viewZenithCosAngle*0.5 + 0.5, (length(worlPos) - Atmosphere.BottomRadius) / (Atmosphere.TopRadius - Atmosphere.BottomRadius)));
    uv = vec2(fromUnitToSubUvs(uv.x, 32), fromUnitToSubUvs(uv.y, 32));

    vec3 multiScatteredLuminance = textureLod(multi_scat, uv, 0).rgb;
    return multiScatteredLuminance;
}

// - r0: ray origin
// - rd: normalized ray direction
// - s0: sphere center
// - sR: sphere radius
// - Returns distance from r0 to first intersecion with sphere,
//   or -1.0 if no intersection.
float raySphereIntersectNearest(vec3 r0, vec3 rd, vec3 s0, float sR) {
    float a = dot(rd, rd);
    vec3 s0_r0 = r0 - s0;
    float b = 2.0 * dot(rd, s0_r0);
    float c = dot(s0_r0, s0_r0) - (sR * sR);
    float delta = b * b - 4.0*a*c;
    if (delta < 0.0 || a == 0.0)
    {
        return -1.0;
    }
    float sol0 = (-b - sqrt(delta)) / (2.0*a);
    float sol1 = (-b + sqrt(delta)) / (2.0*a);
    if (sol0 < 0.0 && sol1 < 0.0)
    {
        return -1.0;
    }
    if (sol0 < 0.0)
    {
        return max(0.0, sol1);
    }
    else if (sol1 < 0.0)
    {
        return max(0.0, sol0);
    }
    return max(0.0, min(sol0, sol1));
}

struct MediumSampleRGB {
    vec3 scattering;
    vec3 absorption;
    vec3 extinction;

    vec3 scatteringMie;
    vec3 absorptionMie;
    vec3 extinctionMie;

    vec3 scatteringRay;
    vec3 absorptionRay;
    vec3 extinctionRay;

    vec3 scatteringOzo;
    vec3 absorptionOzo;
    vec3 extinctionOzo;

    vec3 albedo;
};

MediumSampleRGB sampleMediumRGB(vec3 WorldPos, AtmosphereParameters Atmosphere) {
    const float viewHeight = length(WorldPos) - Atmosphere.BottomRadius;

    const float densityMie = exp(Atmosphere.MieDensityExpScale * viewHeight);
    const float densityRay = exp(Atmosphere.RayleighDensityExpScale * viewHeight);
    const float densityOzo = saturate(viewHeight < Atmosphere.AbsorptionDensity0LayerWidth ?
        Atmosphere.AbsorptionDensity0LinearTerm * viewHeight + Atmosphere.AbsorptionDensity0ConstantTerm :
        Atmosphere.AbsorptionDensity1LinearTerm * viewHeight + Atmosphere.AbsorptionDensity1ConstantTerm);

    MediumSampleRGB s;

    s.scatteringMie = densityMie * Atmosphere.MieScattering;
    s.absorptionMie = densityMie * Atmosphere.MieAbsorption;
    s.extinctionMie = densityMie * Atmosphere.MieExtinction;

    s.scatteringRay = densityRay * Atmosphere.RayleighScattering;
    s.absorptionRay = vec3(0);
    s.extinctionRay = s.scatteringRay + s.absorptionRay;

    s.scatteringOzo = vec3(0);
    s.absorptionOzo = densityOzo * Atmosphere.AbsorptionExtinction;
    s.extinctionOzo = s.scatteringOzo + s.absorptionOzo;

    s.scattering = s.scatteringMie + s.scatteringRay + s.scatteringOzo;
    s.absorption = s.absorptionMie + s.absorptionRay + s.absorptionOzo;
    s.extinction = s.extinctionMie + s.extinctionRay + s.extinctionOzo;
    s.albedo = getAlbedo(s.scattering, s.extinction);

    return s;
}


struct SingleScatteringResult
{
    vec3 L;						// Scattered light (luminance)
    vec3 OpticalDepth;			// Optical depth (1/m)
    vec3 Transmittance;			// Transmittance in [0,1] (unitless)
    vec3 MultiScatAs1;

    vec3 NewMultiScatStep0Out;
    vec3 NewMultiScatStep1Out;
};


SingleScatteringResult IntegrateScatteredLuminance(
    vec2 pixPos,  vec3 WorldPos,  vec3 WorldDir,  vec3 SunDir,  AtmosphereParameters Atmosphere,
    bool ground,  float SampleCountIni,  float DepthBufferValue,  bool VariableSampleCount,
    bool MieRayPhase,  float tMaxMax,
    sampler2D trans_lut,
#if MULTISCATAPPROX_ENABLED
    sampler2D multi_scat,
#endif
    vec2 gResolution,
    mat4 pv_inv
) {
    const bool debugEnabled = false;
    SingleScatteringResult result;
    result.MultiScatAs1 = vec3(0.0);

    vec3 ClipSpace = vec3((pixPos / gResolution)*vec2(2.0, -2.0) - vec2(1.0, -1.0), 1.0);

    // Compute next intersection with atmosphere or ground
    vec3 earthO = vec3(0);
    float tBottom = raySphereIntersectNearest(WorldPos, WorldDir, earthO, Atmosphere.BottomRadius);
    float tTop = raySphereIntersectNearest(WorldPos, WorldDir, earthO, Atmosphere.TopRadius);
    float tMax = 0.0;
    if (tBottom < 0.0)
    {
        if (tTop < 0.0)
        {
            tMax = 0.0; // No intersection with earth nor atmosphere: stop right away
            return result;
        }
        else
        {
            tMax = tTop;
        }
    }
    else
    {
        if (tTop > 0.0)
        {
            tMax = min(tTop, tBottom);
        }
    }

    if (DepthBufferValue >= 0.0)
    {
        ClipSpace.z = DepthBufferValue;
        if (ClipSpace.z < 1.0)
        {
            vec4 DepthBufferWorldPos = pv_inv * vec4(ClipSpace, 1.0);
            DepthBufferWorldPos /= DepthBufferWorldPos.w;

            float tDepth = length(DepthBufferWorldPos.xyz - (WorldPos + vec3(0.0, 0.0, -Atmosphere.BottomRadius))); // apply earth offset to go back to origin as top of earth mode.
            if (tDepth < tMax)
            {
                tMax = tDepth;
            }
        }
        //		if (VariableSampleCount && ClipSpace.z == 1.0)
        //			return result;
    }
    tMax = min(tMax, tMaxMax);

    // Sample count
    float SampleCount = SampleCountIni;
    float SampleCountFloor = SampleCountIni;
    float tMaxFloor = tMax;
    if (VariableSampleCount)
    {
        SampleCount = mix(4, 126, clamp(tMax*0.01, 0, 1));
        SampleCountFloor = floor(SampleCount);
        tMaxFloor = tMax * SampleCountFloor / SampleCount;	// rescale tMax to map to the last entire step segment.
    }
    float dt = tMax / SampleCount;

    // Phase functions
    const float uniformPhase = 1.0 / (4.0 * PI);
    const vec3 wi = SunDir;
    const vec3 wo = WorldDir;
    float cosTheta = dot(wi, wo);
    float MiePhaseValue = hgPhase(Atmosphere.MiePhaseG, -cosTheta);	// mnegate cosTheta because due to WorldDir being a "in" direction.
    float RayleighPhaseValue = RayleighPhase(cosTheta);
    vec3 globalL = vec3(1.0);

    // Ray march the atmosphere to integrate optical depth
    vec3 L = vec3(0.0);
    vec3 throughput = vec3(1.0);
    vec3 OpticalDepth = vec3(0.0);
    float t = 0.0;
    float tPrev = 0.0;
    const float SampleSegmentT = 0.3;
    for (float s = 0.0; s < SampleCount; s += 1.0)
    {
        if (VariableSampleCount)
        {
            // More expenssive but artefact free
            float t0 = (s) / SampleCountFloor;
            float t1 = (s + 1.0) / SampleCountFloor;
            // Non linear distribution of sample within the range.
            t0 = t0 * t0;
            t1 = t1 * t1;
            // Make t0 and t1 world space distances.
            t0 = tMaxFloor * t0;
            if (t1 > 1.0)
            {
                t1 = tMax;
                //	t1 = tMaxFloor;	// this reveal depth slices
            }
            else
            {
                t1 = tMaxFloor * t1;
            }
            //t = t0 + (t1 - t0) * (whangHashNoise(pixPos.x, pixPos.y, gFrameId * 1920 * 1080)); // With dithering required to hide some sampling artefact relying on TAA later? This may even allow volumetric shadow?
            t = t0 + (t1 - t0)*SampleSegmentT;
            dt = t1 - t0;
        }
        else
        {
            //t = tMax * (s + SampleSegmentT) / SampleCount;
            // Exact difference, important for accuracy of multiple scattering
            float NewT = tMax * (s + SampleSegmentT) / SampleCount;
            dt = NewT - t;
            t = NewT;
        }
        vec3 P = WorldPos + t * WorldDir;

        MediumSampleRGB medium = sampleMediumRGB(P, Atmosphere);
        const vec3 SampleOpticalDepth = medium.extinction * dt;
        const vec3 SampleTransmittance = exp(-SampleOpticalDepth);
        OpticalDepth += SampleOpticalDepth;

        float pHeight = length(P);
        const vec3 UpVector = P / pHeight;
        float SunZenithCosAngle = dot(SunDir, UpVector);
        vec2 uv = LutTransmittanceParamsToUv(Atmosphere, pHeight, SunZenithCosAngle);
        uv = clamp(uv, vec2(0.0), vec2(1.0));
        vec3 TransmittanceToSun = textureLod(trans_lut, uv, 0).xyz;

        vec3 PhaseTimesScattering;
        if (MieRayPhase)
        {
            PhaseTimesScattering = medium.scatteringMie * MiePhaseValue + medium.scatteringRay * RayleighPhaseValue;
        }
        else
        {
            PhaseTimesScattering = medium.scattering * uniformPhase;
        }

        // Earth shadow
        float tEarth = raySphereIntersectNearest(P, SunDir, earthO + PLANET_RADIUS_OFFSET * UpVector, Atmosphere.BottomRadius);
        float earthShadow = tEarth >= 0.0 ? 0.0 : 1.0;

        // Dual scattering for multi scattering

        vec3 multiScatteredLuminance = vec3(0.0);
#if MULTISCATAPPROX_ENABLED
        multiScatteredLuminance = GetMultipleScattering(Atmosphere, multi_scat, medium.scattering, medium.extinction, P, SunZenithCosAngle);
#endif

        float shadow = 1.0;
#if SHADOWMAP_ENABLED
        // First evaluate opaque shadow
        shadow = getShadow(Atmosphere, P);
#endif

        vec3 S = globalL * (earthShadow * shadow * TransmittanceToSun * PhaseTimesScattering + multiScatteredLuminance * medium.scattering);

        // When using the power serie to accumulate all sattering order, serie r must be <1 for a serie to converge.
        // Under extreme coefficient, MultiScatAs1 can grow larger and thus result in broken visuals.
        // The way to fix that is to use a proper analytical integration as proposed in slide 28 of http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/
        // However, it is possible to disable as it can also work using simple power serie sum unroll up to 5th order. The rest of the orders has a really low contribution.
#define MULTI_SCATTERING_POWER_SERIE 1

#if MULTI_SCATTERING_POWER_SERIE==0
        // 1 is the integration of luminance over the 4pi of a sphere, and assuming an isotropic phase function of 1.0/(4*PI)
        result.MultiScatAs1 += throughput * medium.scattering * 1 * dt;
#else
        vec3 MS = medium.scattering * 1;
        vec3 MSint = (MS - MS * SampleTransmittance) / medium.extinction;
        result.MultiScatAs1 += throughput * MSint;
#endif

        // Evaluate input to multi scattering
        {
            vec3 newMS;

            newMS = earthShadow * TransmittanceToSun * medium.scattering * uniformPhase * 1;
            result.NewMultiScatStep0Out += throughput * (newMS - newMS * SampleTransmittance) / medium.extinction;
            //	result.NewMultiScatStep0Out += SampleTransmittance * throughput * newMS * dt;

            newMS = medium.scattering * uniformPhase * multiScatteredLuminance;
            result.NewMultiScatStep1Out += throughput * (newMS - newMS * SampleTransmittance) / medium.extinction;
            //	result.NewMultiScatStep1Out += SampleTransmittance * throughput * newMS * dt;
        }

#if 0
        L += throughput * S * dt;
        throughput *= SampleTransmittance;
#else
        // See slide 28 at http://www.frostbite.com/2015/08/physically-based-unified-volumetric-rendering-in-frostbite/
        vec3 Sint = (S - S * SampleTransmittance) / medium.extinction;	// integrate along the current step segment
        L += throughput * Sint;														// accumulate and also take into account the transmittance from previous steps
        throughput *= SampleTransmittance;
#endif

        tPrev = t;
    }

    if (ground && tMax == tBottom && tBottom > 0.0)
    {
        // Account for bounced light off the earth
        vec3 P = WorldPos + tBottom * WorldDir;
        float pHeight = length(P);

        const vec3 UpVector = P / pHeight;
        float SunZenithCosAngle = dot(SunDir, UpVector);
        vec2 uv = LutTransmittanceParamsToUv(Atmosphere, pHeight, SunZenithCosAngle);
        uv = clamp(uv, vec2(0.0), vec2(1.0));
        vec3 TransmittanceToSun = textureLod(trans_lut, uv, 0).xyz;

        const float NdotL = clamp(dot(normalize(UpVector), normalize(SunDir)), 0, 1);
        L += globalL * TransmittanceToSun * throughput * NdotL * Atmosphere.GroundAlbedo / PI;
    }

    result.L = L;
    result.OpticalDepth = OpticalDepth;
    result.Transmittance = throughput;
    return result;
}
