
struct AtmosphereParameters {
    // Radius of the planet (center to ground)
    float BottomRadius;
    // Maximum considered atmosphere height (center to atmosphere top)
    float TopRadius;

    // Rayleigh scattering exponential distribution scale in the atmosphere
    float RayleighDensityExpScale;
    // Rayleigh scattering coefficients
    vec3 RayleighScattering;

    // Mie scattering exponential distribution scale in the atmosphere
    float MieDensityExpScale;
    // Mie scattering coefficients
    vec3 MieScattering;
    // Mie extinction coefficients
    vec3 MieExtinction;
    // Mie absorption coefficients
    vec3 MieAbsorption;
    // Mie phase function excentricity
    float MiePhaseG;

    // Another medium type in the atmosphere
    float AbsorptionDensity0LayerWidth;
    float AbsorptionDensity0ConstantTerm;
    float AbsorptionDensity0LinearTerm;
    float AbsorptionDensity1ConstantTerm;
    float AbsorptionDensity1LinearTerm;
    // This other medium only absorb light, e.g. useful to represent ozone in the earth atmosphere
    vec3 AbsorptionExtinction;

    // The albedo of the ground.
    vec3 GroundAlbedo;
};

AtmosphereParameters GetAtmosphereParameters() {
    AtmosphereParameters Parameters;
    Parameters.AbsorptionExtinction = vec3(0.00065, 0.00188, 0.00008);

    // Traslation from Bruneton2017 parameterisation.
    Parameters.RayleighDensityExpScale = -0.125;
    Parameters.MieDensityExpScale = -0.83333;
    Parameters.AbsorptionDensity0LayerWidth = 25;
    Parameters.AbsorptionDensity0ConstantTerm = -0.66667;
    Parameters.AbsorptionDensity0LinearTerm = 0.06667;
    Parameters.AbsorptionDensity1ConstantTerm = 2.66667;
    Parameters.AbsorptionDensity1LinearTerm = -0.06667;

    Parameters.MiePhaseG = 0.8;
    Parameters.RayleighScattering = vec3(0.0058, 0.01356, 0.0331);
    Parameters.MieScattering = vec3(0.004);
    Parameters.MieAbsorption = vec3(0.00044);
    Parameters.MieExtinction = vec3(0.00444);
    Parameters.GroundAlbedo = vec3(0);
    Parameters.BottomRadius = 6360;
    Parameters.TopRadius = 6460;
    return Parameters;
}
