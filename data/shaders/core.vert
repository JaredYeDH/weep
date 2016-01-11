
layout(location = ATTR_POSITION) in vec3 position;
layout(location = ATTR_TEXCOORD) in vec2 texcoord;
layout(location = ATTR_NORMAL) in vec3 normal;
layout(location = ATTR_COLOR) in vec4 color;
#ifdef USE_SKINNING
layout(location = ATTR_BONE_INDEX) in vec4 boneIndices;
layout(location = ATTR_BONE_WEIGHT) in vec4 boneWeights;
#endif

VERTEX_DATA(out, outData);

void main()
{
	vec4 pos = vec4(position, 1.0);
	vec3 n = normal;
#ifdef USE_SKINNING
	mat3x4 m = boneMatrices[int(boneIndices.x)] * boneWeights.x;
	m += boneMatrices[int(boneIndices.y)] * boneWeights.y;
	m += boneMatrices[int(boneIndices.z)] * boneWeights.z;
	m += boneMatrices[int(boneIndices.w)] * boneWeights.w;
	pos = vec4(pos * m, 1.0);
	n = (vec4(n, 0.0) * m).xyz;
#endif // USE_SKINNING

#if !defined(USE_TESSELLATION) && !defined(USE_CUBE_RENDER)
	gl_Position = modelViewProjMatrix * pos;
#elif !defined(USE_TESSELLATION) && defined(USE_CUBE_RENDER)
	gl_Position = modelMatrix * pos;
#endif
	outData.normal = mat3(normalMatrix) * n;
	outData.position = (modelViewMatrix * pos).xyz;
	outData.texcoord = texcoord * material.uvRepeat + material.uvOffset;
#ifdef USE_SHADOW_MAP
	outData.worldPosition = (modelMatrix * pos).xyz;
	outData.shadowcoord = shadowMatrix * pos;
#endif
}

