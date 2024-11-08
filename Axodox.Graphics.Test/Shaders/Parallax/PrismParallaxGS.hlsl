struct output_t
{
    float4 Position : SV_POSITION;
    float3 localPos : POSITION;
    float2 planeCoord : PLANECOORD;
};
struct input_t
{
    float2 planeCoord : PLANECOORD;
    float3 localPosBottom : POSITION0;
    float3 localPosTop : POSITION1;
    float4 ScreenPosBottom : ScreenPos0;
    float4 ScreenPosTop : ScreenPos1;
};
cbuffer ModelBuffer : register(b1)
{
    float4x4 mMatrix;
};

void top(out output_t dat, in input_t inp)
{
    dat.planeCoord = inp.planeCoord;
    dat.localPos = inp.localPosTop;
    dat.Position = inp.ScreenPosTop;
}
void bot(out output_t dat, in input_t inp)
{
    dat.planeCoord = inp.planeCoord;
    dat.localPos = inp.localPosBottom;
    dat.Position = inp.ScreenPosBottom;
}

// no bottom face so 5 face *3 vertex 
[maxvertexcount(15)]
void main(in InputPatch<input_t, 4> input, inout TriangleStream<output_t> TriStream)
{

    // TODO: 
    // none of this is optimised, but
    // the compiler SHOULD be able to see that we are setting values
    // and probably optimise the whole array out
    
    output_t dat[3];

    
    // Top face
    top(dat[0], input[0]);
    top(dat[1], input[2]);
    top(dat[2], input[1]);
    TriStream.Append(dat[0]);
    TriStream.Append(dat[1]);
    TriStream.Append(dat[2]);
    
    top(dat[0], input[0]);
    top(dat[1], input[3]);
    top(dat[2], input[2]);
    TriStream.Append(dat[0]);
    TriStream.Append(dat[2]);
    TriStream.Append(dat[1]);

    // Emit the side faces
    for (int i = 0; i < 4; i++)
    {
        int next = (i + 1) % 4;
        
        // First triangle of the side face
        bot(dat[0], input[i]);
        bot(dat[1], input[next]);
        top(dat[2], input[next]);
        TriStream.Append(dat[0]);
        TriStream.Append(dat[1]);
        TriStream.Append(dat[2]);

        // Second triangle of the side face
        bot(dat[0], input[i]);
        top(dat[1], input[next]);
        top(dat[2], input[i]);
        TriStream.Append(dat[0]);
        TriStream.Append(dat[1]);
        TriStream.Append(dat[2]);
    }

    // Finish the stream
    TriStream.RestartStrip();
}
