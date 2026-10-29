struct ConstantBufferData
{
	matrix View;
	matrix Projection;

	float4 StartColor;
	float4 EndColor;

	float CurrentTime;
	float3 Acceleration;

	int SpriteSheetWidth;
	int SpriteSheetHeight;
	float SpriteSheetFrameWidth;
	float SpriteSheetFrameHeight;

	float SpriteSheetSpeedScale;
	float StartSize;
	float EndSize;
	float Lifetime;

	float3 ColorTint;
	int ConstrainYAxis;
};

cbuffer DrawData : register(b0)
{
	uint ParticleCBIndex;
	uint ParticleDataIndex;
	uint ParticleTextureIndex;
	uint DebugWireframe;
};

// Struct representing a single particle
// Note: the organization is due to 16-byte alignment!
struct Particle
{
	float EmitTime;
	float3 StartPosition;

	float3 StartVelocity;
	float StartRotation;

	float EndRotation;
	float3 padding;
};

// Defines the output data of our vertex shader
struct VertexToPixel_Particle
{
	float4 position		: SV_POSITION;
	float2 uv           : TEXCOORD0;
	float4 colorTint	: COLOR;
};


// The entry point for our vertex shader
VertexToPixel_Particle main(uint id : SV_VertexID)
{
	// Grab the constant buffer
	ConstantBuffer<ConstantBufferData> cb = ResourceDescriptorHeap[ParticleCBIndex];
	
	// Set up output
	VertexToPixel_Particle output;

	// Get id info
	uint particleID = id / 4; // Every group of 4 verts are ONE particle!
	uint cornerID = id % 4; // 0,1,2,3 = the corner of the particle "quad"

	// Grab one particle and its starting position
	StructuredBuffer<Particle> ParticleData = ResourceDescriptorHeap[ParticleDataIndex];
	Particle p = ParticleData.Load(particleID);

	// Calculate the age and age "percentage" (0 to 1)
	float age = cb.CurrentTime - p.EmitTime;
	float agePercent = age / cb.Lifetime;

	// Constant accleration function to determine the particle's
	// current location based on age, start velocity and accel
	float3 pos = cb.Acceleration * age * age / 2.0f + p.StartVelocity * age + p.StartPosition;

	// Size interpolation
	float size = lerp(cb.StartSize, cb.EndSize, agePercent);

	// Offsets for the 4 corners of a quad - we'll only
	// use one for each vertex, but which one depends
	// on the cornerID above.
	float2 offsets[4];
	offsets[0] = float2(-1.0f, +1.0f);  // TL
	offsets[1] = float2(+1.0f, +1.0f);  // TR
	offsets[2] = float2(+1.0f, -1.0f);  // BR
	offsets[3] = float2(-1.0f, -1.0f);  // BL
	
	// Handle rotation - get sin/cos and build a rotation matrix
	float s, c, rotation = lerp(p.StartRotation, p.EndRotation, agePercent);
	sincos(rotation, s, c); // One function to calc both sin and cos
	float2x2 rot =
	{
		c, s,
		-s, c
	};

	// Rotate the offset for this corner and apply size
	float2 rotatedOffset = mul(offsets[cornerID], rot) * size;

	// Billboarding!
	// Offset the position based on the camera's right and up vectors
	pos += float3(cb.View._11, cb.View._12, cb.View._13) * rotatedOffset.x; // RIGHT
	pos += (cb.ConstrainYAxis ? float3(0,1,0) : float3(cb.View._21, cb.View._22, cb.View._23)) * rotatedOffset.y; // UP

	// Calculate output position
	matrix viewProj = mul(cb.Projection, cb.View);
	output.position = mul(viewProj, float4(pos, 1.0f));

	// Sprite sheet animation calculations
	// Note: Probably even better to swap shaders here (animated vs. non-animated)
	//  but this should work for the demo, as we can think of a non-animated particle
	//  as having a sprite sheet with exactly one frame
	float animPercent = fmod(agePercent * cb.SpriteSheetSpeedScale, 1.0f);
	uint ssIndex = (uint)floor(animPercent * (cb.SpriteSheetWidth * cb.SpriteSheetHeight));

	// Get the U/V indices (basically column & row index across the sprite sheet)
	uint uIndex = ssIndex % cb.SpriteSheetWidth;
	uint vIndex = ssIndex / cb.SpriteSheetWidth; // Integer division is important here!

	// Convert to a top-left corner in uv space (0-1)
	float u = uIndex / (float)cb.SpriteSheetWidth;
	float v = vIndex / (float)cb.SpriteSheetHeight;

	float2 uvs[4];
	/* TL */ uvs[0] = float2(u, v);
	/* TR */ uvs[1] = float2(u + cb.SpriteSheetFrameWidth, v);
	/* BR */ uvs[2] = float2(u + cb.SpriteSheetFrameWidth, v + cb.SpriteSheetFrameHeight);
	/* BL */ uvs[3] = float2(u, v + cb.SpriteSheetFrameHeight);
	
	// Finalize output
	output.uv = saturate(uvs[cornerID]);
	output.colorTint = lerp(cb.StartColor, cb.EndColor, agePercent);
	output.colorTint.rgb *= cb.ColorTint;

	return output;
}
	