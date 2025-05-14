# Bruneton Atmosphere

> Eric Bruneton, Fabrice Neyret. Precomputed Atmospheric Scattering. Computer Graphics Forum, 2008, Special Issue: Proceedings of the 19th Eurographics Symposium on Rendering 2008, 27 (4), pp.1079-1086. ⟨10.1111/j.1467-8659.2008.01245.x⟩. ⟨inria-00288758⟩

### Rayleigh scattering coefficient

Describe interaction of air molecules and light.

$$\beta^s_R (h, \lambda) = \frac{8\pi^3 (n^2-1)^2}{3N\lambda^4} e^{-\frac{h}{H_R}}$$

where

* $R_g = 6360 \ km$ : Ground level radius
* $R_t = 6420 \ km$ : Upper bound of atmosphere
* $h = r-R_g$ : Altitude
* $\lambda = (680,550,440) \ nm$ : Wavelength
* $n$ : Index of refraction of air
* $N$ : Molecular density at sea level $R_g$
* $H_R = 8 \ km$ : Thickness of the armosphere if density were uniform

However, do not evaluate the equation but just use $\beta^s_R = (5.8,13.5,33.1)10^{-6} \ m^{-1}$ for each wavelengths $\lambda = (680,550,440) \ nm$.

### Rayleigh phase function

$$P_R(\mu) = \frac{3}{16\pi} (1+\mu^2)$$

### Rayleigh extinction coefficient

$$\beta^e_R = \beta^s_R$$

### Mie scattering coefficient

Describe interaction of air aerosols and light.

$$\beta^s_M (h, \lambda) = \beta^s_M (0, \lambda)e^{-\frac{h}{H_M}}$$

where

* $H_M \simeq 1.2 \ km$

### Mie phase function

$$P_M(\mu) = \frac{3}{8\pi} \frac{(1-g^2)(1+\mu^2)}{(2+g^2)(1+g^2-2g\mu)^\frac{3}{2}}$$

### Mie extinction coefficient

$$\beta^e_M = \beta^s_M + \beta^a_M$$

As per figure 6 in [Bruneton, 2018]

* $\beta^s_M = 210^{-5} \ m^{-1}$
* ${\beta^s_M} / {\beta^e_M} = 0.9$

### Rendering equation

* $L(\textbf{x}, \textbf{v}, \textbf{s})$ : Radiance of light
    - $\textbf{x}$ : Sample point
    - $\textbf{v}$ : Direction where the light comes from
    - $\textbf{s}$ : Direction of the sun

* $\textbf{x}_o(\textbf{x}, \textbf{v})$ : Extremity of ray $\textbf{x} + t\textbf{v}$, which is either on the ground or on the top atmosphere boundary $r = R_t$

### Transmittance

*Transmittance* between $\textbf{x}_o$ and $\textbf{x}$ is

$$T(\textbf{x}, \textbf{x}_o) = exp( -\int_{\textbf{x}}^{\textbf{x}_o} \sum_{i \in \{R,M\}} \beta^e_i (\textbf{y})dy)$$

### Radiance of light reflected at $\textbf{x}_o$

$$\mathcal{I}[L](\textbf{x}_o, \textbf{s}) = \frac{\alpha(\textbf{x}_o)}{\pi} \int_{2\pi} L(\textbf{x}_o, \omega, \textbf{s})\omega.\mathbf{n}(\mathbf{x}_o)d\omega$$

$\mathcal{I}$ is null on the top of atmosphere.

### Radiance of light scattered at $y$ in direction $-v$

$$\mathcal{J}[L](y, v, s) = \int_{4pi}
 \sum_{i} \beta^S_i(y) \ ... $$

### Light reflected at x_o and attenuated before reaching x

$$\mathcal{R}[L](\textbf{x}, \textbf{v}, \textbf{s}) = T(\textbf{x}, \textbf{x}_o) \mathcal{I}[L] (\textbf{x}_o,\textbf{s})$$

### Inscattered light

$$\mathcal{S}[L](\textbf{x}, \textbf{v}, \textbf{s})$$

The light scattered towards x between x and x_o

### Direct sunlight attenuated before reaching $x$ by $T$

$$L_0(\textbf{x}, \textbf{v}, \textbf{s}) = T(\textbf{x}, \textbf{x}_o)L_{sun}, \ or \ 0$$

It's $0$ if $\textbf{v} \neq \textbf{s}$.

### Rendering equation

$$L(\textbf{x}, \textbf{v}, \textbf{s}) = (L_0 + \mathcal{R}[L] + \mathcal{s}[L])(\textbf{x}, \textbf{v}, \textbf{s})$$

Ignoring multiple scattering results in following

$$L = L_0 + \mathcal{R}[L_0] + \mathcal{s}[L_0]$$
