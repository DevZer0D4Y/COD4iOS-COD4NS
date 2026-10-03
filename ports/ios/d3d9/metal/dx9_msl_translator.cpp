#include "dx9_msl_translator.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <set>

namespace kisak::metal {

namespace {

// D3DSIO_* opcodes.
enum : uint32_t
{
    OpNop = 0, OpMov = 1, OpAdd = 2, OpSub = 3, OpMad = 4, OpMul = 5, OpRcp = 6, OpRsq = 7, OpDp3 = 8, OpDp4 = 9,
    OpMin = 10, OpMax = 11, OpSlt = 12, OpSge = 13, OpExp = 14, OpLog = 15, OpLit = 16, OpDst = 17, OpLrp = 18,
    OpFrc = 19, OpM4x4 = 20, OpM4x3 = 21, OpM3x4 = 22, OpM3x3 = 23, OpM3x2 = 24, OpCall = 25, OpCallNz = 26,
    OpLoop = 27, OpRet = 28, OpEndLoop = 29, OpLabel = 30, OpDcl = 31, OpPow = 32, OpCrs = 33, OpSgn = 34,
    OpAbs = 35, OpNrm = 36, OpSinCos = 37, OpRep = 38, OpEndRep = 39, OpIf = 40, OpIfc = 41, OpElse = 42,
    OpEndIf = 43, OpBreak = 44, OpBreakC = 45, OpMova = 46, OpDefB = 47, OpDefI = 48,
    OpTexCoord = 64, OpTexKill = 65, OpTex = 66, OpCnd = 80, OpDef = 81, OpCmp = 88, OpDp2Add = 90, OpDsx = 91,
    OpDsy = 92, OpTexLdd = 93, OpTexLdl = 95,
    OpComment = 0xFFFE, OpEnd = 0xFFFF,
};

// D3DSPR_* register types.
enum : uint32_t
{
    RegTemp = 0, RegInput = 1, RegConst = 2, RegTexture = 3, RegRastOut = 4, RegAttrOut = 5, RegOutput = 6,
    RegConstInt = 7, RegColorOut = 8, RegDepthOut = 9, RegSampler = 10, RegConstBool = 14, RegLoop = 15,
    RegMiscType = 17, RegLabel = 18, RegPredicate = 19,
};

uint32_t RegisterType(uint32_t token) { return ((token >> 28) & 0x7) | ((token >> 8) & 0x18); }
uint32_t RegisterNumber(uint32_t token) { return token & 0x7FF; }
bool IsRelative(uint32_t token) { return (token & 0x2000) != 0; }

const char *UsageName(uint8_t usage)
{
    switch (usage)
    {
    case kUsagePosition: return "position";
    case kUsageBlendWeight: return "blendweight";
    case kUsageBlendIndices: return "blendindices";
    case kUsageNormal: return "normal";
    case kUsagePointSize: return "psize";
    case kUsageTexCoord: return "texcoord";
    case kUsageTangent: return "tangent";
    case kUsageBinormal: return "binormal";
    case kUsageTessFactor: return "tessfactor";
    case kUsagePositionT: return "positiont";
    case kUsageColor: return "color";
    case kUsageFog: return "fog";
    case kUsageDepth: return "depth";
    case kUsageSample: return "sample";
    default: return "usage";
    }
}

std::string SemanticName(uint8_t usage, uint8_t index)
{
    return std::string(UsageName(usage)) + std::to_string(index);
}

std::string FloatLiteral(float value)
{
    if (value != value)
        return "NAN";
    if (value == std::numeric_limits<float>::infinity())
        return "INFINITY";
    if (value == -std::numeric_limits<float>::infinity())
        return "(-INFINITY)";
    char buffer[64];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, std::chars_format::general, 9);
    std::string text(buffer, result.ptr);
    if (text.find_first_of(".e") == std::string::npos)
        text += ".0";
    return text;
}

int VaryingBit(uint8_t usage, uint8_t index)
{
    switch (usage)
    {
    case kUsageTexCoord: return index < 8 ? index : -1;
    case kUsageColor: return index < 2 ? 8 + index : -1;
    case kUsageFog: return index == 0 ? 10 : -1;
    case kUsageNormal: return index == 0 ? 11 : -1;
    case kUsageTangent: return index == 0 ? 12 : -1;
    case kUsageBinormal: return index == 0 ? 13 : -1;
    default: return -1;
    }
}

struct Instruction
{
    uint32_t opcode = 0;
    uint32_t controls = 0;
    const uint32_t *params = nullptr;
    uint32_t length = 0;
};

class Translator
{
public:
    Translator(const uint32_t *tokens, size_t count, const TranslateOptions &options, TranslatedShader &out)
        : m_tokens(tokens), m_count(count), m_options(options), m_out(out)
    {
    }

    bool Run()
    {
        if (m_count < 2)
            return Fail("program too short");
        const uint32_t version = m_tokens[0];
        m_out.pixel = (version >> 16) == 0xFFFF;
        if (!m_out.pixel && (version >> 16) != 0xFFFE)
            return Fail("unknown shader version token");
        if (((version >> 8) & 0xFF) != 3)
            return Fail("only shader model 3 is supported");
        if (!Decode() || !Collect() || !Emit())
            return false;
        return true;
    }

private:
    bool Fail(const std::string &message)
    {
        if (m_out.error.empty())
            m_out.error = message;
        return false;
    }

    bool Decode()
    {
        size_t pos = 1;
        while (pos < m_count)
        {
            const uint32_t token = m_tokens[pos];
            const uint32_t opcode = token & 0xFFFF;
            if (opcode == OpEnd)
                return true;
            if (opcode == OpComment)
            {
                pos += 1 + ((token >> 16) & 0x7FFF);
                continue;
            }
            Instruction instruction;
            instruction.opcode = opcode;
            instruction.controls = (token >> 16) & 0xFF;
            instruction.length = (token >> 24) & 0x0F;
            instruction.params = m_tokens + pos + 1;
            if (pos + 1 + instruction.length > m_count)
                return Fail("instruction runs past the end of the program");
            m_instructions.push_back(instruction);
            pos += 1 + instruction.length;
        }
        return Fail("missing end token");
    }

    bool Collect()
    {
        for (const Instruction &in : m_instructions)
        {
            if (in.opcode == OpDcl)
            {
                if (in.length < 2)
                    return Fail("malformed dcl");
                const uint32_t usageToken = in.params[0];
                const uint32_t reg = in.params[1];
                const uint32_t type = RegisterType(reg);
                const uint16_t number = static_cast<uint16_t>(RegisterNumber(reg));
                if (type == RegSampler)
                {
                    ShaderSampler sampler;
                    sampler.reg = number;
                    sampler.type = static_cast<uint8_t>((usageToken >> 27) & 0xF);
                    m_out.samplers.push_back(sampler);
                    continue;
                }
                ShaderSemantic semantic;
                semantic.usage = static_cast<uint8_t>(usageToken & 0x1F);
                semantic.index = static_cast<uint8_t>((usageToken >> 16) & 0xF);
                semantic.reg = number;
                if (type == RegInput)
                    m_out.inputs.push_back(semantic);
                else if (type == RegOutput)
                    m_out.outputs.push_back(semantic);
                else if (type == RegMiscType)
                    return Fail("vPos/vFace inputs are not supported");
                else
                    return Fail("dcl on unsupported register type " + std::to_string(type));
            }
            else if (in.opcode == OpDef)
            {
                if (in.length < 5)
                    return Fail("malformed def");
                std::array<float, 4> value{};
                for (int i = 0; i < 4; ++i)
                    memcpy(&value[i], &in.params[1 + i], sizeof(float));
                m_defs[RegisterNumber(in.params[0])] = value;
            }
            else if (in.opcode == OpDefI)
            {
                if (in.length < 5)
                    return Fail("malformed defi");
                std::array<int32_t, 4> value{};
                for (int i = 0; i < 4; ++i)
                    value[i] = static_cast<int32_t>(in.params[1 + i]);
                m_defIs[RegisterNumber(in.params[0])] = value;
            }
            else if (in.opcode == OpDefB)
            {
                // Boolean constants only feed static branches.
            }
            else
            {
                for (uint32_t i = 0; i < in.length; ++i)
                    NoteRegister(in.params[i]);
            }
        }
        return m_out.error.empty();
    }

    void NoteRegister(uint32_t token)
    {
        const uint32_t type = RegisterType(token);
        const uint32_t number = RegisterNumber(token);
        switch (type)
        {
        case RegTemp: m_maxTemp = std::max(m_maxTemp, number + 1); break;
        case RegColorOut: m_colorOutputs.insert(number); break;
        case RegRastOut: m_rastOutputs.insert(number); break;
        case RegAttrOut: m_attrOutputs.insert(number); break;
        case RegOutput: m_maxOutput = std::max(m_maxOutput, number + 1); break;
        case RegInput: m_maxInput = std::max(m_maxInput, number + 1); break;
        default: break;
        }
    }

    // ---------------------------------------------------------------------
    // Expressions

    std::string RegisterName(uint32_t token, uint32_t offset = 0)
    {
        if (IsRelative(token))
        {
            Fail("relative addressing is not supported");
            return "float4(0)";
        }
        const uint32_t type = RegisterType(token);
        const uint32_t number = RegisterNumber(token) + offset;
        switch (type)
        {
        case RegTemp: return "r" + std::to_string(number);
        case RegInput: return "v" + std::to_string(number);
        case RegConst:
            if (m_defs.count(number))
                return "k" + std::to_string(number);
            m_out.constantCount = std::max(m_out.constantCount, number + 1);
            return "C[" + std::to_string(number) + "]";
        case RegOutput: return "o" + std::to_string(number);
        case RegColorOut: return "oC" + std::to_string(number);
        case RegRastOut: return number == 0 ? "oPos" : number == 1 ? "oFog" : "oPts";
        case RegAttrOut: return "oD" + std::to_string(number);
        default:
            Fail("unsupported register type " + std::to_string(type));
            return "float4(0)";
        }
    }

    std::string Source(uint32_t token)
    {
        std::string expr = RegisterName(token);
        const uint32_t swizzle = (token >> 16) & 0xFF;
        if (swizzle != 0xE4)
        {
            static const char components[] = "xyzw";
            expr += '.';
            for (int i = 0; i < 4; ++i)
                expr += components[(swizzle >> (2 * i)) & 3];
        }
        switch ((token >> 24) & 0xF)
        {
        case 0: return expr;
        case 1: return "(-" + expr + ")";
        case 2: return "(" + expr + " - 0.5)";
        case 3: return "(0.5 - " + expr + ")";
        case 4: return "(2.0 * " + expr + " - 1.0)";
        case 5: return "(1.0 - 2.0 * " + expr + ")";
        case 6: return "(1.0 - " + expr + ")";
        case 7: return "(2.0 * " + expr + ")";
        case 8: return "(-2.0 * " + expr + ")";
        case 11: return "abs(" + expr + ")";
        case 12: return "(-abs(" + expr + "))";
        default:
            Fail("unsupported source modifier " + std::to_string((token >> 24) & 0xF));
            return expr;
        }
    }

    std::string MaskString(uint32_t token)
    {
        static const char components[] = "xyzw";
        std::string mask;
        const uint32_t bits = (token >> 16) & 0xF;
        for (int i = 0; i < 4; ++i)
            if (bits & (1u << i))
                mask += components[i];
        return mask;
    }

    void Assign(uint32_t destToken, const std::string &value)
    {
        const std::string name = RegisterName(destToken);
        const bool saturate = (((destToken >> 20) & 0xF) & 0x1) != 0;
        const std::string expr = saturate ? "saturate(" + value + ")" : value;
        const std::string mask = MaskString(destToken);
        if (mask.empty())
            return;
        if (mask == "xyzw")
            Line(name + " = " + expr + ";");
        else
            Line(name + "." + mask + " = (" + expr + ")." + mask + ";");
    }

    void Line(const std::string &text)
    {
        m_body.append(static_cast<size_t>(m_depth + 1) * 4, ' ');
        m_body += text;
        m_body += '\n';
    }

    bool VaryingAvailable(const ShaderSemantic &input) const
    {
        const int bit = VaryingBit(input.usage, input.index);
        return bit < 0 || ((m_options.availableVaryings >> bit) & 1);
    }

    const ShaderSampler *FindSampler(uint32_t reg) const
    {
        for (const ShaderSampler &sampler : m_out.samplers)
            if (sampler.reg == reg)
                return &sampler;
        return nullptr;
    }

    std::string Sample(const Instruction &in)
    {
        const uint32_t samplerToken = in.params[2];
        if (RegisterType(samplerToken) != RegSampler)
        {
            Fail("texture sample without a sampler register");
            return "float4(0)";
        }
        const uint32_t reg = RegisterNumber(samplerToken);
        const ShaderSampler *sampler = FindSampler(reg);
        const uint8_t type = sampler ? sampler->type : kTexture2D;
        const std::string coord = Source(in.params[1]);
        const std::string tex = "tex" + std::to_string(reg);
        const std::string smp = "smp" + std::to_string(reg);
        const bool depth = type != kTextureCube && type != kTextureVolume && (m_options.depthSamplerMask >> reg) & 1;
        const std::string swizzle = [&] {
            const uint32_t sw = (samplerToken >> 16) & 0xFF;
            if (sw == 0xE4)
                return std::string();
            static const char components[] = "xyzw";
            std::string text = ".";
            for (int i = 0; i < 4; ++i)
                text += components[(sw >> (2 * i)) & 3];
            return text;
        }();

        std::string args;
        if (in.opcode == OpTexLdl || !m_out.pixel)
            args = ", level(" + coord + ".w)";
        else if (in.opcode == OpTexLdd)
        {
            const std::string dx = Source(in.params[3]);
            const std::string dy = Source(in.params[4]);
            if (type == kTextureCube)
                args = ", gradientcube(" + dx + ".xyz, " + dy + ".xyz)";
            else if (type == kTextureVolume)
                args = ", gradient3d(" + dx + ".xyz, " + dy + ".xyz)";
            else
                args = ", gradient2d(" + dx + ".xy, " + dy + ".xy)";
        }
        else if (in.controls & 2)
            args = ", bias(" + coord + ".w)";

        const bool project = in.opcode == OpTex && (in.controls & 1);
        std::string result;
        if (depth)
        {
            const std::string uv = project ? "(" + coord + ".xy / " + coord + ".w)" : coord + ".xy";
            const std::string ref = project ? "(" + coord + ".z / " + coord + ".w)" : coord + ".z";
            const std::string compareArgs = in.opcode == OpTexLdl || !m_out.pixel ? ", level(0)" : "";
            result = "float4(" + tex + ".sample_compare(" + smp + ", " + uv + ", " + ref + compareArgs + "))";
        }
        else if (type == kTextureCube)
            result = tex + ".sample(" + smp + ", " + coord + ".xyz" + args + ")";
        else if (type == kTextureVolume)
            result = tex + ".sample(" + smp + ", " + (project ? "(" + coord + ".xyz / " + coord + ".w)" : coord + ".xyz") + args + ")";
        else
            result = tex + ".sample(" + smp + ", " + (project ? "(" + coord + ".xy / " + coord + ".w)" : coord + ".xy") + args + ")";
        return result + swizzle;
    }

    std::string MatrixRows(const Instruction &in, int rows, int columns)
    {
        const std::string vec = Source(in.params[1]);
        std::string components = columns == 4 ? "" : ".xyz";
        std::string result = "float4(";
        for (int row = 0; row < 4; ++row)
        {
            if (row)
                result += ", ";
            if (row < rows)
                result += "dot(" + vec + components + ", " + RegisterName(in.params[2], static_cast<uint32_t>(row)) + components + ")";
            else
                result += "0.0";
        }
        return result + ")";
    }

    bool EmitInstruction(const Instruction &in)
    {
        auto src = [&](uint32_t i) { return Source(in.params[i]); };
        const uint32_t dest = in.length ? in.params[0] : 0;
        switch (in.opcode)
        {
        case OpNop:
        case OpDcl:
        case OpDef:
        case OpDefI:
        case OpDefB:
            return true;
        case OpMov: Assign(dest, src(1)); return true;
        case OpAdd: Assign(dest, src(1) + " + " + src(2)); return true;
        case OpSub: Assign(dest, src(1) + " - " + src(2)); return true;
        case OpMul: Assign(dest, src(1) + " * " + src(2)); return true;
        case OpMad: Assign(dest, src(1) + " * " + src(2) + " + " + src(3)); return true;
        case OpRcp: Assign(dest, "float4(1.0 / " + src(1) + ".x)"); return true;
        case OpRsq: Assign(dest, "float4(rsqrt(abs(" + src(1) + ".x)))"); return true;
        case OpDp3: Assign(dest, "float4(dot(" + src(1) + ".xyz, " + src(2) + ".xyz))"); return true;
        case OpDp4: Assign(dest, "float4(dot(" + src(1) + ", " + src(2) + "))"); return true;
        case OpMin: Assign(dest, "min(" + src(1) + ", " + src(2) + ")"); return true;
        case OpMax: Assign(dest, "max(" + src(1) + ", " + src(2) + ")"); return true;
        case OpSlt: Assign(dest, "select(float4(0.0), float4(1.0), " + src(1) + " < " + src(2) + ")"); return true;
        case OpSge: Assign(dest, "select(float4(0.0), float4(1.0), " + src(1) + " >= " + src(2) + ")"); return true;
        case OpExp: Assign(dest, "float4(exp2(" + src(1) + ".x))"); return true; // D3D9 exp is 2^x (log is log2)
        case OpLog: Assign(dest, "float4(log2(abs(" + src(1) + ".x)))"); return true;
        case OpLrp: Assign(dest, "mix(" + src(3) + ", " + src(2) + ", " + src(1) + ")"); return true;
        case OpFrc: Assign(dest, "fract(" + src(1) + ")"); return true;
        case OpM4x4: Assign(dest, MatrixRows(in, 4, 4)); return true;
        case OpM4x3: Assign(dest, MatrixRows(in, 3, 4)); return true;
        case OpM3x4: Assign(dest, MatrixRows(in, 4, 3)); return true;
        case OpM3x3: Assign(dest, MatrixRows(in, 3, 3)); return true;
        case OpPow: Assign(dest, "float4(pow(abs(" + src(1) + ".x), " + src(2) + ".x))"); return true;
        case OpCrs: Assign(dest, "float4(cross(" + src(1) + ".xyz, " + src(2) + ".xyz), 0.0)"); return true;
        case OpSgn: Assign(dest, "sign(" + src(1) + ")"); return true;
        case OpAbs: Assign(dest, "abs(" + src(1) + ")"); return true;
        case OpNrm: Assign(dest, src(1) + " * rsqrt(max(dot(" + src(1) + ".xyz, " + src(1) + ".xyz), 1e-30))"); return true;
        case OpSinCos: Assign(dest, "float4(cos(" + src(1) + ".x), sin(" + src(1) + ".x), 0.0, 0.0)"); return true;
        case OpCmp: Assign(dest, "select(" + src(3) + ", " + src(2) + ", " + src(1) + " >= 0.0)"); return true;
        case OpCnd: Assign(dest, "select(" + src(3) + ", " + src(2) + ", " + src(1) + " > 0.5)"); return true;
        case OpDp2Add: Assign(dest, "float4(dot(" + src(1) + ".xy, " + src(2) + ".xy) + " + src(3) + ".x)"); return true;
        case OpDsx:
            if (!m_out.pixel)
                return Fail("dsx in a vertex shader");
            Assign(dest, "dfdx(" + src(1) + ")");
            return true;
        case OpDsy:
            if (!m_out.pixel)
                return Fail("dsy in a vertex shader");
            Assign(dest, "dfdy(" + src(1) + ")");
            return true;
        case OpTex:
        case OpTexLdl:
            if (in.length < 3)
                return Fail("malformed texture sample");
            Assign(dest, Sample(in));
            return true;
        case OpTexLdd:
            if (in.length < 5)
                return Fail("malformed texldd");
            Assign(dest, Sample(in));
            return true;
        case OpTexKill:
        {
            if (!m_out.pixel)
                return Fail("texkill in a vertex shader");
            Line("if (any(" + RegisterName(dest) + ".xyz < 0.0))");
            Line("    discard_fragment();");
            return true;
        }
        case OpIfc:
        {
            static const char *const compares[] = {"?", ">", "==", ">=", "<", "!=", "<="};
            const uint32_t func = in.controls & 0x7;
            if (func < 1 || func > 6)
                return Fail("bad ifc comparison");
            Line("if (" + src(0) + ".x " + compares[func] + " " + src(1) + ".x)");
            Line("{");
            ++m_depth;
            return true;
        }
        case OpElse:
            --m_depth;
            Line("}");
            Line("else");
            Line("{");
            ++m_depth;
            return true;
        case OpEndIf:
            --m_depth;
            Line("}");
            return true;
        case OpRep:
        {
            const uint32_t token = in.params[0];
            if (RegisterType(token) != RegConstInt)
                return Fail("rep on a non-integer register");
            const auto def = m_defIs.find(RegisterNumber(token));
            if (def == m_defIs.end())
                return Fail("rep count comes from an undefined integer constant");
            const std::string counter = "rep" + std::to_string(m_repCount++);
            const int count = std::clamp(def->second[0], 0, 255);
            Line("for (int " + counter + " = 0; " + counter + " < " + std::to_string(count) + "; ++" + counter + ")");
            Line("{");
            ++m_depth;
            return true;
        }
        case OpEndRep:
            --m_depth;
            Line("}");
            return true;
        case OpBreak:
            Line("break;");
            return true;
        default:
            return Fail("unsupported opcode " + std::to_string(in.opcode));
        }
    }

    // ---------------------------------------------------------------------
    // Program

    std::string SamplerParameters()
    {
        std::string params;
        for (const ShaderSampler &sampler : m_out.samplers)
        {
            const std::string reg = std::to_string(sampler.reg);
            const bool depth = sampler.type != kTextureCube && sampler.type != kTextureVolume
                            && (m_options.depthSamplerMask >> sampler.reg) & 1;
            const char *textureType = depth ? "depth2d<float>"
                                    : sampler.type == kTextureCube ? "texturecube<float>"
                                    : sampler.type == kTextureVolume ? "texture3d<float>"
                                    : "texture2d<float>";
            params += ",\n    " + std::string(textureType) + " tex" + reg + " [[texture(" + reg + ")]]";
            params += ",\n    sampler smp" + reg + " [[sampler(" + reg + ")]]";
        }
        return params;
    }

    std::string Declarations()
    {
        std::string text;
        for (uint32_t i = 0; i < m_maxTemp; ++i)
            text += "    float4 r" + std::to_string(i) + " = float4(0.0);\n";
        for (const auto &[reg, value] : m_defs)
            text += "    const float4 k" + std::to_string(reg) + " = float4(" + FloatLiteral(value[0]) + ", " + FloatLiteral(value[1])
                  + ", " + FloatLiteral(value[2]) + ", " + FloatLiteral(value[3]) + ");\n";
        return text;
    }

    bool EmitBody()
    {
        for (const Instruction &in : m_instructions)
            if (!EmitInstruction(in))
                return false;
        if (m_depth != 0)
            return Fail("unbalanced if/endif");
        return m_out.error.empty();
    }

    bool Emit()
    {
        std::string &s = m_out.source;
        s = "#include <metal_stdlib>\nusing namespace metal;\n\n";
        return m_out.pixel ? EmitPixel() : EmitVertex();
    }

    bool EmitVertex()
    {
        if (!EmitBody())
            return false;
        std::string &s = m_out.source;
        s += "struct KisakVertexParams\n{\n    float2 halfPixel;\n};\n\n";

        bool haveInputs = false;
        std::string inputStruct = "struct VertexIn\n{\n";
        for (const ShaderSemantic &input : m_out.inputs)
        {
            const int slot = AttributeSlot(input.usage, input.index);
            if (slot < 0)
                return Fail("vertex input " + SemanticName(input.usage, input.index) + " has no attribute slot");
            const char *attributeType = ((m_options.unsignedIntInputMask >> slot) & 1) ? "uint4"
                                      : ((m_options.signedIntInputMask >> slot) & 1)   ? "int4"
                                                                                         : "float4";
            inputStruct += "    " + std::string(attributeType) + " " + SemanticName(input.usage, input.index) + " [[attribute(" + std::to_string(slot) + ")]];\n";
            haveInputs = true;
        }
        inputStruct += "};\n\n";

        const ShaderSemantic *position = nullptr;
        std::string outputStruct = "struct VertexOut\n{\n    float4 position [[position]];\n";
        for (const ShaderSemantic &output : m_out.outputs)
        {
            if (output.usage == kUsagePosition && output.index == 0)
            {
                position = &output;
                continue;
            }
            if (output.usage == kUsagePointSize)
            {
                outputStruct += "    float pointSize [[point_size]];\n";
                continue;
            }
            const std::string name = SemanticName(output.usage, output.index);
            outputStruct += "    float4 " + name + " [[user(" + name + ")]];\n";
        }
        outputStruct += "};\n\n";
        if (!position)
            return Fail("vertex shader has no position output");

        if (haveInputs)
            s += inputStruct;
        s += outputStruct;
        s += "vertex VertexOut kisak_main(";
        std::string params;
        if (haveInputs)
            params += "\n    VertexIn in [[stage_in]],";
        params += "\n    constant float4 *C [[buffer(" + std::to_string(kVertexConstantBuffer) + ")]],";
        params += "\n    constant KisakVertexParams &params [[buffer(" + std::to_string(kVertexParamsBuffer) + ")]]";
        params += SamplerParameters();
        s += params + ")\n{\n";
        s += Declarations();
        for (uint32_t i = 0; i < m_maxInput; ++i)
            s += "    float4 v" + std::to_string(i) + " = float4(0.0, 0.0, 0.0, 1.0);\n";
        for (const ShaderSemantic &input : m_out.inputs)
            s += "    v" + std::to_string(input.reg) + " = float4(in." + SemanticName(input.usage, input.index) + ");\n";
        for (uint32_t i = 0; i < m_maxOutput; ++i)
            s += "    float4 o" + std::to_string(i) + " = float4(0.0);\n";
        s += "\n" + m_body + "\n    VertexOut out;\n";
        for (const ShaderSemantic &output : m_out.outputs)
        {
            const std::string reg = "o" + std::to_string(output.reg);
            if (&output == position)
                s += "    out.position = " + reg + ";\n";
            else if (output.usage == kUsagePointSize)
                s += "    out.pointSize = " + reg + ".x;\n";
            else
                s += "    out." + SemanticName(output.usage, output.index) + " = " + reg + ";\n";
        }
        s += "    out.position.xy += params.halfPixel * out.position.w;\n";
        s += "    return out;\n}\n";
        return true;
    }

    bool EmitPixel()
    {
        if (!EmitBody())
            return false;
        std::string &s = m_out.source;
        s += "struct KisakFragmentParams\n{\n    int alphaFunc;\n    float alphaRef;\n};\n\n";

        bool haveInputs = false;
        std::string inputStruct = "struct FragmentIn\n{\n";
        for (const ShaderSemantic &input : m_out.inputs)
        {
            if (input.usage == kUsagePosition)
                return Fail("pixel shader position input is not supported");
            if (!VaryingAvailable(input))
                continue;
            const std::string name = SemanticName(input.usage, input.index);
            inputStruct += "    float4 " + name + " [[user(" + name + ")]];\n";
            haveInputs = true;
        }
        inputStruct += "};\n\n";
        if (haveInputs)
            s += inputStruct;

        if (m_colorOutputs.empty())
            m_colorOutputs.insert(0);
        s += "struct FragmentOut\n{\n";
        for (uint32_t index : m_colorOutputs)
            s += "    float4 oC" + std::to_string(index) + " [[color(" + std::to_string(index) + ")]];\n";
        s += "};\n\n";

        s += "fragment FragmentOut kisak_main(";
        std::string params;
        if (haveInputs)
            params += "\n    FragmentIn in [[stage_in]],";
        params += "\n    constant float4 *C [[buffer(" + std::to_string(kFragmentConstantBuffer) + ")]],";
        params += "\n    constant KisakFragmentParams &params [[buffer(" + std::to_string(kFragmentParamsBuffer) + ")]]";
        params += SamplerParameters();
        s += params + ")\n{\n";
        s += Declarations();
        for (uint32_t i = 0; i < m_maxInput; ++i)
            s += "    float4 v" + std::to_string(i) + " = float4(0.0);\n";
        for (const ShaderSemantic &input : m_out.inputs)
            if (VaryingAvailable(input))
                s += "    v" + std::to_string(input.reg) + " = in." + SemanticName(input.usage, input.index) + ";\n";
        for (uint32_t index : m_colorOutputs)
            s += "    float4 oC" + std::to_string(index) + " = float4(0.0);\n";
        s += "\n" + m_body + "\n";
        // Fixed-function alpha test (D3DRS_ALPHATESTENABLE/ALPHAFUNC/ALPHAREF).
        s += "    if (params.alphaFunc != 0)\n    {\n";
        s += "        const float a = oC0.a;\n        const float ref = params.alphaRef;\n";
        s += "        bool pass = true;\n";
        s += "        switch (params.alphaFunc)\n        {\n";
        s += "        case 1: pass = false; break;\n";
        s += "        case 2: pass = a < ref; break;\n";
        s += "        case 3: pass = a == ref; break;\n";
        s += "        case 4: pass = a <= ref; break;\n";
        s += "        case 5: pass = a > ref; break;\n";
        s += "        case 6: pass = a != ref; break;\n";
        s += "        case 7: pass = a >= ref; break;\n";
        s += "        default: break;\n        }\n";
        s += "        if (!pass)\n            discard_fragment();\n    }\n";
        s += "    FragmentOut out;\n";
        for (uint32_t index : m_colorOutputs)
            s += "    out.oC" + std::to_string(index) + " = oC" + std::to_string(index) + ";\n";
        s += "    return out;\n}\n";
        return true;
    }

    const uint32_t *m_tokens;
    size_t m_count;
    const TranslateOptions &m_options;
    TranslatedShader &m_out;
    std::vector<Instruction> m_instructions;
    std::map<uint32_t, std::array<float, 4>> m_defs;
    std::map<uint32_t, std::array<int32_t, 4>> m_defIs;
    int m_repCount = 0;
    std::set<uint32_t> m_colorOutputs;
    std::set<uint32_t> m_rastOutputs;
    std::set<uint32_t> m_attrOutputs;
    uint32_t m_maxTemp = 0;
    uint32_t m_maxInput = 0;
    uint32_t m_maxOutput = 0;
    int m_depth = 0;
    std::string m_body;
};

} // namespace

int AttributeSlot(uint8_t usage, uint8_t index)
{
    switch (usage)
    {
    case kUsagePosition:
    case kUsagePositionT:
        return index == 0 ? 0 : -1;
    case kUsageNormal: return index == 0 ? 1 : -1;
    case kUsageColor: return index < 2 ? 2 + index : -1;
    case kUsageTexCoord: return index < 8 ? 4 + index : -1;
    case kUsageTangent: return index == 0 ? 12 : -1;
    case kUsageBinormal: return index == 0 ? 13 : -1;
    case kUsageBlendWeight: return index == 0 ? 14 : -1;
    case kUsageBlendIndices: return index == 0 ? 15 : -1;
    case kUsagePointSize: return index == 0 ? 16 : -1;
    default: return -1;
    }
}

bool TranslateShader(const uint32_t *tokens, size_t tokenCount, const TranslateOptions &options, TranslatedShader &out)
{
    out = TranslatedShader{};
    if (!tokens)
    {
        out.error = "no program";
        return false;
    }
    Translator translator(tokens, tokenCount, options, out);
    return translator.Run() && out.error.empty();
}

} // namespace kisak::metal
