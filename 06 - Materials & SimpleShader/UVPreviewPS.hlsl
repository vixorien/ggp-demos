
// Struct representing the data we expect to receive from earlier pipeline stages
struct VertexToPixel
{
	float4 screenPosition : SV_POSITION;
	float2 uv : TEXCOORD;
	float3 normal : NORMAL;
};

// Returns the incoming UV data as a color
float4 main(VertexToPixel input) : SV_TARGET
{
	return float4(input.uv, 0, 1);
}