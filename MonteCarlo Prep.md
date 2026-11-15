# Monte Carlo Direct-Lighting Plan for Our Vulkan Path Tracer
**Project:** CSCI 580: PathTracer / ReSTIR
**Purpose:** Document a concrete first step toward Monte Carlo path tracing, referencing Michael's initial `Setup`.

## 1. What I'm looking into

Michael's README already covers how the Vulkan renderer is set up, so I won't repeat all of that here. From the current code, we can cast rays into the Cornell box and get information about what they hit, but the shader is still mainly showing surface normals instead of calculating actual lighting for starter code check.

I think the simplest next step is to start with **Monte Carlo direct-light sampling**. Once we have that working, it can also serve as a baseline for comparing regular light sampling with ReSTIR. **This is our proposed next step, based on the current renderer and what ReSTIR is designed to improve.**

## 2. Monte Carlo light sampling
general idea is that instead of calculating lighting from every possible direction, we take random samples and use their average to estimate the lighting at a point (Shirley et al., 2025).

For our Cornell box, there is an area light on the ceiling which we could randomly choose points on that light, check whether each point is visible from the surface, and estimate how much light reaches it. This would follow the general idea of sampling area lights to calculate direct illumination (Pharr et al. 2016).

It could look something along the lines of this roughly: 
1.  Shoot a camera ray and find the surface it hits.
2.  pick a random point on the ceiling light.
3.  Find the dir and dist from the surface to that point.
4.  Cast a shadow ray to see whether something blocks the light.
5.  If /= blocked, calculate the light contribution.
6.  Repeat this a few times and average the results.

##### *(based on Shirley et al 2025 & Pharr et al. 2016)*

Since this is only a few samples, the image will probably look a bit noisy. We can increase the number of samples to make it more stable, but a con is that it also takes more work to render. 

### The Math 
For the first version, I think we can keep the material diffuse (Lambertian) to make the calculation easier. The formula below is an area-light Monte Carlo estimator, using the geometry and PDF terms discussed in the light-sampling references (Pharr et al., 2016; Shirley et al., 2025).

![Direct Lighting Monte Carlo Estimator](https://i.ibb.co/35CFhpfx/how-many-light-samples-we-take.jpg)


Where:

-   `N` is how many light samples we take.
-   `f_r` is the diffuse material response (`baseColor / pi`).
-   `L_e` is how much light the sampled area light emits.
-   `V` is 1 if the light is visible and 0 if it's blocked.
-   The cosine terms account for the angles of the surface and light.
-   `d` is the distance between the surface and the sampled light point.
-   `p_A` is the probability density of choosing that point on the light. With uniform area sampling, `p_A = 1 / lightArea`.

> Dividing by the probability density function (PDF) is important because it accounts for how likely each light sample is to be selected. Without this weighting system, the lighting estimate could become biased and produce brightness that can be incorrect (Pharr et al., 2016; Shirley et al., 2025).
>   <p align="center"> Cornel box ceiling light with 4 square units </p>

> $$
p_A = \frac{1}{\text{Area}} = \frac{1}{4} = 0.25
$$
> <p align="center"> Suppose the lighting contribution of a sampled point is 2. </p>



> $$
\frac{\text{Contribution}}{\text{PDF}}
= \frac{2}{0.25}
= 8
$$

>We divide by the PDF to account for how the sample was selected.
>
>If some light samples are selected more frequently than others, dividing by their PDF prevents those regions from incorrectly contributing too much brightness.
## 3. Rough pseudocode
This is just for **one surface hit and direct lighting**. It isn't full path tracing with multiple bounces yet. Pseudocode is a suggestion on first implementation for our project. It is **not copied code** from a paper or tutorial. It is based on the sampling steps and probability weighting explained by Pharr et al. (2016) and Shirley et al. (2025).
function estimateDirectLighting(hit, sampleCount):
    totalLighting = (0, 0, 0)

    repeat sampleCount times:
        // Choose a random point on the ceiling area light
        lightPoint = sampleCeilingLight()
        direction = normalize(lightPoint - hit.position)
        distance = length(lightPoint - hit.position)

        surfaceCos = max(0, dot(hit.normal, direction))
        lightCos = max(0, dot(lightNormal, -direction))

        if surfaceCos <= 0 or lightCos <= 0:
            continue

        // Make sure the light isn't blocked by another object
        if traceShadowRay(hit.position, lightPoint) is blocked:
            continue

        diffuse = hit.baseColor / PI
        pdfArea = 1 / lightArea
        geometry = (surfaceCos * lightCos) / (distance * distance)

        contribution = diffuse * lightEmission * geometry / pdfArea
        totalLighting += contribution

    return totalLighting / sampleCount

We would also need a small offset when starting shadow rays so the surface doesn't accidentally hit itself (Pharr et al., 2016 & also Professor Slocum :D ). This is meant to show the algorithm first, not to be ready-to-compile Slang code.

## 4. Where this could go in our current project
Based on the current repo and Michael's README that he setup
-   **[`shaders/integrators/pathtrace_mc.slang`](https://github.com/michaelk0217/PathTracerProject/blob/main/shaders/integrators/pathtrace_mc.slang)** — This looks like the main place to add the light sampling. `rayGen()` starts the rays, and the hit payload gives us the position, normal, and material we need.
-   **`shadowMiss()`** — This already exists as a shader entry point, so we should be able to use shadow rays for visibility checks.
-   **[`src/scene/SceneLoader.cpp`](https://github.com/michaelk0217/PathTracerProject/blob/main/src/scene/SceneLoader.cpp)** — The Cornell box already has a ceiling area light that we can use for the first test.
-   **[`shaders/shared/GpuStructs.h`](https://github.com/michaelk0217/PathTracerProject/blob/main/shaders/shared/GpuStructs.h)** — The material information includes base color and emission, which the light calculation will need.
### Possible implementation order

1.  Add a random-number function in Slang, using a different seed per pixel/sample.
2.  Sample the existing ceiling light and calculate basic diffuse lighting.
3.  Add shadow-ray checks so objects can cast shadows.
4.  Try different sample counts (like 1, 4, 16, and 64) and see how much the noise changes.
5.  Later, add more lights and use this as the regular Monte Carlo baseline for ReSTIR.

## 5. How this connects to ReSTIR
So the first comparison we'd want to make is **regular direct-light sampling vs. ReSTIR direct-light sampling**, with the same scene and roughly the same budgets.

>The Cornell box is a good starting point for checking that the lighting math works. However, since it only has one area light, we'll probably need a scene with more lights to really show where ReSTIR helps. That's our proposed test plan, motivated by the many-light examples in the ReSTIR sources (Anagnostou, 2023; Bitterli et al., 2020).

## 6. References

Anagnostou, K. (2023, December 17). _A gentler introduction to ReSTIR_. Interplay of Light. [https://interplayoflight.wordpress.com/2023/12/17/a-gentler-introduction-to-restir/](https://interplayoflight.wordpress.com/2023/12/17/a-gentler-introduction-to-restir/)

Bitterli, B., Wyman, C., Pharr, M., Shirley, P., Lefohn, A., & Jarosz, W. (2020). Spatiotemporal reservoir resampling for real-time ray tracing with dynamic direct lighting. _ACM Transactions on Graphics, 39_(4), Article 148. [https://doi.org/10.1145/3386569.3392481](https://doi.org/10.1145/3386569.3392481)


Pharr, M., Jakob, W., & Humphreys, G. (2016). _Physically based rendering: From theory to implementation_ (3rd ed.). Morgan Kaufmann. [https://www.pbr-book.org/3ed-2018/Light_Transport_I_Surface_Reflection/Sampling_Light_Sources](https://www.pbr-book.org/3ed-2018/Light_Transport_I_Surface_Reflection/Sampling_Light_Sources)

Shirley, P., Black, T. D., & Hollasch, S. (2025). _Ray tracing: The rest of your life_ (Version 4.0.2). [https://raytracing.github.io/books/RayTracingTheRestOfYourLife.html](https://raytracing.github.io/books/RayTracingTheRestOfYourLife.html)
