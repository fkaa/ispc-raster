struct float2 {
    float x;
    float y;
};

struct float3 {
    float x;
    float y;
    float z;
};

struct float4 {
    float x;
    float y;
    float z;
    float w;
};

struct float4x4 {
    float m[4][4];
};

static float vec3_length(struct float3 vec);
static float vec3_dot(struct float3 a, struct float3 b);
static struct float3 vec3_normalize(struct float3 vec);
static struct float3 vec3_cross(struct float3 a, struct float3 b);
static struct float3 vec3_sub(struct float3 a, struct float3 b);
static struct float3 vec3_mul(struct float3 a, struct float3 b);
static struct float3 vec3_mul_scalar(struct float3 a, float s);

static struct float4 vec4_adds(struct float4 a, float s);
static struct float4 vec4_muls(struct float4 a, float s);
static struct float4 vec4_transform(struct float4 a, struct float4x4 b);

static struct float4x4 mat4_identity();
static struct float4x4 mat4_mul(struct float4x4 a, struct float4x4 b);
static struct float4x4 mat4_perspective_RH(float fov, float aspect, float n, float f);
static struct float4x4 mat4_look_at_RH(struct float3 pos, struct float3 target, struct float3 up);

static float vec3_dot(struct float3 a, struct float3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static float vec3_length(struct float3 vec)
{
    return sqrtf(vec3_dot(vec, vec));
}

static struct float3 vec3_normalize(struct float3 vec)
{
    return vec3_mul_scalar(vec, 1.f / vec3_length(vec));
}

static struct float3 vec3_cross(struct float3 a, struct float3 b)
{
    return (struct float3) { .x = (a.y * b.z - a.z * b.y) , .y = (a.z * b.x - a.x * b.z), .z = (a.x * b.y - a.y * b.x) };
}

static struct float3 vec3_sub(struct float3 a, struct float3 b)
{
    return (struct float3) { a.x - b.x, a.y - b.y, a.z - b.z };
}

static struct float3 vec3_mul(struct float3 a, struct float3 b)
{
    return (struct float3) { a.x * b.x, a.y * b.y, a.z * b.z };
}


static struct float3 vec3_mul_scalar(struct float3 a, float s)
{
    return (struct float3) { a.x * s, a.y * s, a.z * s };
}

static struct float4 vec4_adds(struct float4 a, float s)
{
    return (struct float4) { a.x + s, a.y + s, a.z + s, a.w + s };
}

static struct float4 vec4_muls(struct float4 a, float s)
{
    return (struct float4) { a.x * s, a.y * s, a.z * s, a.w * s };
}

static struct float4 vec4_transform(struct float4 a, struct float4x4 b)
{
    return (struct float4) {
        (a.x * b.m[0][0]) + (a.y * b.m[1][0]) + (a.z * b.m[2][0]) + (a.w * b.m[3][0]),
        (a.x * b.m[0][1]) + (a.y * b.m[1][1]) + (a.z * b.m[2][1]) + (a.w * b.m[3][1]),
        (a.x * b.m[0][2]) + (a.y * b.m[1][2]) + (a.z * b.m[2][2]) + (a.w * b.m[3][2]),
        (a.x * b.m[0][3]) + (a.y * b.m[1][3]) + (a.z * b.m[2][3]) + (a.w * b.m[3][3]),
    };
}

static struct float4x4 mat4_mul(struct float4x4 a, struct float4x4 b)
{
    struct float4x4 m;

    float x = a.m[0][0],
          y = a.m[0][1],
          z = a.m[0][2],
          w = a.m[0][3];

    m.m[0][0] = (b.m[0][0] * x) + (b.m[1][0] * y) + (b.m[2][0] * z) + (b.m[3][0] * w);
    m.m[0][1] = (b.m[0][1] * x) + (b.m[1][1] * y) + (b.m[2][1] * z) + (b.m[3][1] * w);
    m.m[0][2] = (b.m[0][2] * x) + (b.m[1][2] * y) + (b.m[2][2] * z) + (b.m[3][2] * w);
    m.m[0][3] = (b.m[0][3] * x) + (b.m[1][3] * y) + (b.m[2][3] * z) + (b.m[3][3] * w);

    x = a.m[1][0];
    y = a.m[1][1];
    z = a.m[1][2];
    w = a.m[1][3];

    m.m[1][0] = (b.m[0][0] * x) + (b.m[1][0] * y) + (b.m[2][0] * z) + (b.m[3][0] * w);
    m.m[1][1] = (b.m[0][1] * x) + (b.m[1][1] * y) + (b.m[2][1] * z) + (b.m[3][1] * w);
    m.m[1][2] = (b.m[0][2] * x) + (b.m[1][2] * y) + (b.m[2][2] * z) + (b.m[3][2] * w);
    m.m[1][3] = (b.m[0][3] * x) + (b.m[1][3] * y) + (b.m[2][3] * z) + (b.m[3][3] * w);

    x = a.m[2][0];
    y = a.m[2][1];
    z = a.m[2][2];
    w = a.m[2][3];

    m.m[2][0] = (b.m[0][0] * x) + (b.m[1][0] * y) + (b.m[2][0] * z) + (b.m[3][0] * w);
    m.m[2][1] = (b.m[0][1] * x) + (b.m[1][1] * y) + (b.m[2][1] * z) + (b.m[3][1] * w);
    m.m[2][2] = (b.m[0][2] * x) + (b.m[1][2] * y) + (b.m[2][2] * z) + (b.m[3][2] * w);
    m.m[2][3] = (b.m[0][3] * x) + (b.m[1][3] * y) + (b.m[2][3] * z) + (b.m[3][3] * w);

    x = a.m[3][0];
    y = a.m[3][1];
    z = a.m[3][2];
    w = a.m[3][3];

    m.m[3][0] = (b.m[0][0] * x) + (b.m[1][0] * y) + (b.m[2][0] * z) + (b.m[3][0] * w);
    m.m[3][1] = (b.m[0][1] * x) + (b.m[1][1] * y) + (b.m[2][1] * z) + (b.m[3][1] * w);
    m.m[3][2] = (b.m[0][2] * x) + (b.m[1][2] * y) + (b.m[2][2] * z) + (b.m[3][2] * w);
    m.m[3][3] = (b.m[0][3] * x) + (b.m[1][3] * y) + (b.m[2][3] * z) + (b.m[3][3] * w);

    return m;
}

static struct float4x4 mat4_identity()
{
    return (struct float4x4) {{
        { 1.f, 0.f, 0.f, 0.f },
        { 0.f, 1.f, 0.f, 0.f },
        { 0.f, 0.f, 1.f, 0.f },
        { 0.f, 0.f, 0.f, 1.f }
    }};
}

static struct float4x4 mat4_perspective_RH(float fov, float aspect, float n, float f)
{
    float sinfov = sinf(fov * 0.5f),
        cosfov = cosf(fov * 0.5f);

    float height = cosfov / sinfov;
    float width  = height * aspect;
    float range = f/ (n - f);
    float rn = -(f * n) / (f - n);

    return (struct float4x4) {{
        { width, 0.f,    0.f,          0.f },
        { 0.f,   height, 0.f,          0.f },
        { 0.f,   0.f,    range,       -1.f },
        { 0.f,   0.f,    rn, 0.f }
    }};
}


static struct float4x4 mat4_look_at_RH(struct float3 pos, struct float3 target, struct float3 up)
{
    struct float3 Z = vec3_normalize(vec3_sub(target, pos));
    struct float3 X = vec3_normalize(vec3_cross(Z, up));
    struct float3 Y = vec3_cross(X, Z);

    return (struct float4x4) {{
        { X.x,               Y.x,               -Z.x,              0.f },
        { X.y,               Y.y,               -Z.y,              0.f },
        { X.z,               Y.z,               -Z.z,              0.f },
        { -vec3_dot(X, pos), -vec3_dot(Y, pos), vec3_dot(Z, pos),  1.f }
    }};
}

static struct float4x4 mat4_viewport(float x, float y, float w, float h)
{
    float depth = 255.f;
    return (struct float4x4) {{
        { w/2.f, 0.f, 0.f, x+w/2.f },
        { 0.f, h/2.f, 0.f, y+h/2.f },
        { 0.f, 0.f, depth/2.f, depth/2.f },
        { 0.f, 0.f, 0.f, 1.f }
    }};
}

