
in VertexData {
	vec3 texcoord;
};

layout(location = 0) out vec4 fragment;
#ifndef USE_CUBE_RENDER
layout(location = 1) out vec4 brightFragment;
#endif

// https://github.com/wwwtyro/glsl-atmosphere (public domain)
#define iSteps 16
#define jSteps 8

float rsi(vec3 r0, vec3 rd, float sr)
{
	// Simplified ray-sphere intersection that assumes
	// the ray starts inside the sphere and that the
	// sphere is centered at the origin. Always intersects.
	float a = dot(rd, rd);
	float b = 2.0 * dot(rd, r0);
	float c = dot(r0, r0) - (sr * sr);
	return (-b + sqrt((b*b) - 4.0*a*c))/(2.0*a);
}

vec3 atmosphere(vec3 r, vec3 r0, vec3 pSun, float iSun, float rPlanet, float rAtmos, vec3 kRlh, float kMie, float shRlh, float shMie, float g)
{
	// Normalize the sun and view directions.
	pSun = normalize(pSun);
	r = normalize(r);

	// Calculate the step size of the primary ray.
	float iStepSize = rsi(r0, r, rAtmos) / float(iSteps);

	// Initialize the primary ray time.
	float iTime = 0.0;

	// Initialize accumulators for Rayleight and Mie scattering.
	vec3 totalRlh = vec3(0,0,0);
	vec3 totalMie = vec3(0,0,0);

	// Initialize optical depth accumulators for the primary ray.
	float iOdRlh = 0.0;
	float iOdMie = 0.0;

	// Calculate the Rayleigh and Mie phases.
	float mu = dot(r, pSun);
	float mumu = mu * mu;
	float gg = g * g;
	float pRlh = 3.0 / (16.0 * PI) * (1.0 + mumu);
	float pMie = 3.0 / (8.0 * PI) * ((1.0 - gg) * (mumu + 1.0)) / (pow(1.0 + gg - 2.0 * mu * g, 1.5) * (2.0 + gg));

	// Sample the primary ray.
	for (int i = 0; i < iSteps; i++) {

		// Calculate the primary ray sample position.
		vec3 iPos = r0 + r * (iTime + iStepSize * 0.5);

		// Calculate the height of the sample.
		float iHeight = length(iPos) - rPlanet;

		// Calculate the optical depth of the Rayleigh and Mie scattering for this step.
		float odStepRlh = exp(-iHeight / shRlh) * iStepSize;
		float odStepMie = exp(-iHeight / shMie) * iStepSize;

		// Accumulate optical depth.
		iOdRlh += odStepRlh;
		iOdMie += odStepMie;

		// Calculate the step size of the secondary ray.
		float jStepSize = rsi(iPos, pSun, rAtmos) / float(jSteps);

		// Initialize the secondary ray time.
		float jTime = 0.0;

		// Initialize optical depth accumulators for the secondary ray.
		float jOdRlh = 0.0;
		float jOdMie = 0.0;

		// Sample the secondary ray.
		for (int j = 0; j < jSteps; j++) {

			// Calculate the secondary ray sample position.
			vec3 jPos = iPos + pSun * (jTime + jStepSize * 0.5);

			// Calculate the height of the sample.
			float jHeight = length(jPos) - rPlanet;

			// Accumulate the optical depth.
			jOdRlh += exp(-jHeight / shRlh) * jStepSize;
			jOdMie += exp(-jHeight / shMie) * jStepSize;

			// Increment the secondary ray time.
			jTime += jStepSize;
		}

		// Calculate attenuation.
		vec3 attn = exp(-(kMie * (iOdMie + jOdMie) + kRlh * (iOdRlh + jOdRlh)));

		// Accumulate scattering.
		totalRlh += odStepRlh * attn;
		totalMie += odStepMie * attn;

		// Increment the primary ray time.
		iTime += iStepSize;

	}

	// Calculate and return the final color.
	return iSun * (pRlh * kRlh * totalRlh + pMie * kMie * totalMie);
}


void main()
{
	if (skyType == 0) {
		fragment = texture(envMap, texcoord);
	} else {
		vec3 sky = atmosphere(
			normalize(texcoord),            // normalized ray direction
			vec3(0, 6372e3, 0),             // ray origin
			sunPosition,                    // position of the sun
			22.0,                           // intensity of the sun
			6371e3,                         // radius of the planet in meters
			6471e3,                         // radius of the atmosphere in meters
			vec3(5.5e-6, 13.0e-6, 22.4e-6), // Rayleigh scattering coefficient
			21e-6,                          // Mie scattering coefficient
			8e3,                            // Rayleigh scale height
			1.2e3,                          // Mie scale height
			0.758                           // Mie preferred scattering direction
		);
		fragment = vec4(sky, 1.0);
	}

#ifndef USE_CUBE_RENDER
	if (bloomThreshold > 0.f) {
		//float brightness = dot(fragment.rgb, vec3(0.2126, 0.7152, 0.0722));
		//brightFragment = brightness > bloomThreshold ? vec4(fragment.rgb, 1.0) : vec4(0.0, 0.0, 0.0, 1.0);
		brightFragment = fragment;
	} else brightFragment = vec4(0.0, 0.0, 0.0, 1.0);
#endif
}
