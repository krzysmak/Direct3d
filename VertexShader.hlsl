cbuffer vs_const_buffer_t {
	float4x4 matWorldViewProj;
	float4 padding[12];	// zapewnia buforowi sta�emu rozmiar 
 					// b�d�cy krotno�ci� 256 bajt�w
};

struct vs_output_t {
	float4 position : SV_POSITION;
	float4 color : COLOR;
};

vs_output_t main(float3 pos : POSITION, float4 col : COLOR) {
	vs_output_t result;
	result.position = mul(float4(pos, 1.0f), matWorldViewProj);
	result.color = col;
	return result;
}
