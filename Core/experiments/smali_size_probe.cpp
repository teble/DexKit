// Opt-in linkage experiment, never part of a normal library build.
// Compare identical bounded instruction subsets before choosing a body writer.
#include <algorithm>
#include <charconv>
#include <cstring>
#include <string>
#include <vector>
#include "slicer/dex_bytecode.h"
#if DEXKIT_SMALI_PROBE_IR
#include "slicer/code_ir.h"
#endif

namespace {
void Number(std::string& out, int64_t value) {
    char buffer[24];
    auto end = std::to_chars(buffer, buffer + sizeof(buffer), value).ptr;
    out.append(buffer, end);
}
void Register(std::string& out, uint32_t reg) { out += 'v'; Number(out, reg); }
void Label(std::string& out, uint32_t offset) { out += ":pc_"; Number(out, offset); }

// Both backends get exactly the same preflight, inputs and output contract.
bool Check(const uint16_t* code, size_t size, uint16_t regs,
           std::vector<uint32_t>& starts, std::vector<uint32_t>& labels) {
    if (!code || size == 0 || size > 65536) return false;
    for (size_t pc = 0; pc < size;) {
        auto op = static_cast<dex::Opcode>(code[pc] & 255);
        switch (op) {
            case dex::OP_NOP: if (code[pc] != 0) return false; break;
            case dex::OP_RETURN_VOID:
            case dex::OP_MOVE:
            case dex::OP_CONST_4:
            case dex::OP_CONST_16:
            case dex::OP_GOTO:
            case dex::OP_IF_EQZ: break;
            default: return false;
        }
        size_t width = dex::GetWidthFromFormat(dex::GetFormatFromOpcode(op));
        if (width > size - pc) return false;
        auto inst = dex::DecodeInstruction(code + pc);
        if (op == dex::OP_MOVE && (inst.vA >= regs || inst.vB >= regs)) return false;
        if ((op == dex::OP_CONST_4 || op == dex::OP_CONST_16 || op == dex::OP_IF_EQZ)
            && inst.vA >= regs) return false;
        if (op == dex::OP_GOTO || op == dex::OP_IF_EQZ) {
            int64_t target = int64_t(pc) + int64_t(static_cast<int32_t>(
                op == dex::OP_GOTO ? inst.vA : inst.vB));
            if (target < 0 || target >= static_cast<int64_t>(size)) return false;
            labels.push_back(static_cast<uint32_t>(target));
        }
        starts.push_back(static_cast<uint32_t>(pc));
        pc += width;
    }
    std::sort(labels.begin(), labels.end());
    labels.erase(std::unique(labels.begin(), labels.end()), labels.end());
    for (auto pc : labels) if (!std::binary_search(starts.begin(), starts.end(), pc)) return false;
    return true;
}

#if DEXKIT_SMALI_PROBE_IR
struct BodyVisitor final : lir::Visitor {
    std::string& out;
    explicit BodyVisitor(std::string& out) : out(out) {}
    bool Visit(lir::Bytecode* code) override {
        out += dex::GetOpcodeName(code->opcode);
        for (size_t i = 0; i < code->operands.size(); ++i) {
            out += i ? ", " : " ";
            if (!code->operands[i]->Accept(this)) return false;
        }
        out += '\n';
        return true;
    }
    bool Visit(lir::VReg* reg) override { Register(out, reg->reg); return true; }
    bool Visit(lir::Const32* value) override { Number(out, value->u.s4_value); return true; }
    bool Visit(lir::CodeLocation* value) override { Label(out, value->label->offset); return true; }
    bool Visit(lir::Label* label) override { Label(out, label->offset); out += '\n'; return true; }
};
#endif

bool Write(const uint16_t* code, size_t size, uint16_t regs, std::string& out) {
    std::vector<uint32_t> starts, labels;
    if (!Check(code, size, regs, starts, labels)) return false;
    out = ".method public static probe()V\n.registers ";
    Number(out, regs);
    out += '\n';
#if DEXKIT_SMALI_PROBE_IR
    auto dex_ir = std::make_shared<ir::DexFile>();
    auto method = dex_ir->Alloc<ir::EncodedMethod>();
    method->code = dex_ir->Alloc<ir::Code>();
    method->code->registers = regs;
    method->code->instructions = {code, size};
    lir::CodeIr body(method, dex_ir);
    BodyVisitor visitor(out);
    for (auto node : body.instructions) if (!node->Accept(&visitor)) return false;
#else
    for (auto pc : starts) {
        if (std::binary_search(labels.begin(), labels.end(), pc)) { Label(out, pc); out += '\n'; }
        auto inst = dex::DecodeInstruction(code + pc);
        out += dex::GetOpcodeName(inst.opcode);
        switch (inst.opcode) {
            case dex::OP_MOVE:
                out += ' '; Register(out, inst.vA); out += ", "; Register(out, inst.vB); break;
            case dex::OP_CONST_4: case dex::OP_CONST_16:
                out += ' '; Register(out, inst.vA); out += ", "; Number(out, int32_t(inst.vB)); break;
            case dex::OP_GOTO:
                out += ' '; Label(out, pc + int32_t(inst.vA)); break;
            case dex::OP_IF_EQZ:
                out += ' '; Register(out, inst.vA); out += ", "; Label(out, pc + int32_t(inst.vB)); break;
            default: break;
        }
        out += '\n';
    }
#endif
    out += ".end method\n";
    return true;
}
} // namespace

// Exported so LTO/section GC cannot discard the experiment. The caller supplies
// a live aligned buffer and output capacity; no pointers escape the call.
extern "C" __attribute__((visibility("default"))) size_t DexKitSmaliSizeProbe(
        const uint16_t* code, size_t size, uint16_t regs, char* output, size_t capacity) {
    std::string text;
    if (!Write(code, size, regs, text)) return 0;
    if (output && capacity >= text.size()) std::memcpy(output, text.data(), text.size());
    return text.size();
}
