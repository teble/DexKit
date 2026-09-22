// Copyright (C) 2026 DexKit contributors. SPDX-License-Identifier: LGPL-3.0-or-later
#include "debug.h"

namespace dexkit::smali {

bool DebugInfo::Index(size_t& cursor, uint32_t& value, uint32_t count) {
    uint32_t encoded;
    if (!dex_.Uleb(cursor, encoded)) return false;
    value = encoded - 1;
    return value == dex::kNoIndex || value < count || state_.Fail(SmaliError::MalformedInput, cursor, value);
}

bool DebugInfo::Add(Event event, size_t cursor) {
    state_.code_offset = event.pc;
    bool position = event.opcode == dex::DBG_FIRST_SPECIAL;
    if (!code_.Boundary(event.pc, !position))
        return state_.Fail(SmaliError::DebugNotRepresentable, cursor, event.opcode);
    if (events_.size() == events_.max_size()) return state_.Fail(SmaliError::LimitExceeded, cursor);
    events_.push_back(event);
    return true;
}

bool DebugInfo::Read(size_t parameter_count) {
    state_.phase = SmaliPhase::Read;
    state_.code_offset = UINT32_MAX;
    parameter_names_.clear();
    events_.clear();
    next_ = 0;
    size_t cursor = code_.header().debug_info_off;
    if (!cursor) return true;
    uint32_t initial_line, parameters;
    if (!dex_.Uleb(cursor, initial_line) || !dex_.Uleb(cursor, parameters)) return false;
    if (parameters != parameter_count) return state_.Fail(SmaliError::MalformedInput, cursor);
    if (!state_.Items(parameters) || parameters > parameter_names_.max_size())
        return state_.Fail(SmaliError::LimitExceeded, cursor);
    parameter_names_.resize(parameters);
    for (auto& name : parameter_names_)
        if (!Index(cursor, name, dex_.header().string_ids_size)) return false;
    if (initial_line > INT32_MAX) return state_.Fail(SmaliError::DebugNotRepresentable, cursor);
    int64_t line = initial_line;
    uint32_t pc = 0;
    while (true) {
        state_.code_offset = pc;
        if (!state_.Items()) return false;
        uint8_t opcode;
        size_t event_offset = cursor;
        if (!dex_.Data(cursor, 1) || !dex_.Object(cursor++, opcode)) return false;
        Event event;
        event.pc = pc;
        event.opcode = opcode;
        switch (opcode) {
            case dex::DBG_END_SEQUENCE: return true;
            case dex::DBG_ADVANCE_PC: {
                uint32_t delta;
                if (!dex_.Uleb(cursor, delta)) return false;
                if (delta > UINT32_MAX - pc) return state_.Fail(SmaliError::MalformedInput, event_offset);
                pc += delta;
                continue;
            }
            case dex::DBG_ADVANCE_LINE: {
                int32_t delta;
                if (!dex_.Sleb(cursor, delta)) return false;
                line += delta;
                if (line < INT32_MIN || line > INT32_MAX)
                    return state_.Fail(SmaliError::DebugNotRepresentable, event_offset);
                continue;
            }
            case dex::DBG_START_LOCAL:
            case dex::DBG_START_LOCAL_EXTENDED: {
                if (!dex_.Uleb(cursor, event.reg) || !Index(cursor, event.name, dex_.header().string_ids_size) ||
                    !Index(cursor, event.type, dex_.header().type_ids_size)) return false;
                if (opcode == dex::DBG_START_LOCAL_EXTENDED &&
                    !Index(cursor, event.signature, dex_.header().string_ids_size)) return false;
                if (event.reg >= code_.header().registers_size)
                    return state_.Fail(SmaliError::MalformedInput, event_offset, event.reg);
                if (event.type != dex::kNoIndex) {
                    std::string type;
                    if (!dex_.Type(event.type, type, true)) return false;
                    if (type == "V") return state_.Fail(SmaliError::DebugNotRepresentable, event_offset);
                    if ((type == "J" || type == "D") && event.reg + 1 >= code_.header().registers_size)
                        return state_.Fail(SmaliError::MalformedInput, event_offset, event.reg);
                }
                break;
            }
            case dex::DBG_END_LOCAL:
            case dex::DBG_RESTART_LOCAL:
                if (!dex_.Uleb(cursor, event.reg)) return false;
                if (event.reg >= code_.header().registers_size)
                    return state_.Fail(SmaliError::MalformedInput, event_offset, event.reg);
                break;
            case dex::DBG_SET_PROLOGUE_END:
            case dex::DBG_SET_EPILOGUE_BEGIN: break;
            case dex::DBG_SET_FILE:
                if (!Index(cursor, event.name, dex_.header().string_ids_size)) return false;
                break;
            default: {
                unsigned adjusted = opcode - dex::DBG_FIRST_SPECIAL;
                unsigned delta = adjusted / dex::DBG_LINE_RANGE;
                if (delta > UINT32_MAX - pc) return state_.Fail(SmaliError::MalformedInput, event_offset);
                pc += delta;
                line += dex::DBG_LINE_BASE + int(adjusted % dex::DBG_LINE_RANGE);
                if (line < INT32_MIN || line > INT32_MAX)
                    return state_.Fail(SmaliError::DebugNotRepresentable, event_offset);
                event.pc = pc;
                event.line = int32_t(line);
                event.opcode = dex::DBG_FIRST_SPECIAL;
                break;
            }
        }
        if (!Add(event, event_offset)) return false;
    }
}

bool DebugInfo::EmitAt(uint32_t pc) {
    state_.phase = SmaliPhase::Emit;
    state_.code_offset = pc;
    while (next_ < events_.size() && events_[next_].pc == pc) {
        const auto& event = events_[next_++];
        switch (event.opcode) {
            case dex::DBG_FIRST_SPECIAL:
                if (!state_.Append(".line ") || !state_.Number(event.line)) return false;
                break;
            case dex::DBG_START_LOCAL:
            case dex::DBG_START_LOCAL_EXTENDED:
                if (!state_.Append(".local v") || !state_.Number(event.reg) || !state_.Append(", ")) return false;
                if (event.name == dex::kNoIndex) { if (!state_.Append("null")) return false; }
                else if (!dex_.Quoted(event.name)) return false;
                if (!state_.Append(":")) return false;
                if (event.type == dex::kNoIndex) { if (!state_.Append("V")) return false; }
                else {
                    std::string type;
                    if (!dex_.Type(event.type, type) || !state_.Append(type)) return false;
                }
                if (event.signature != dex::kNoIndex && (!state_.Append(", ") || !dex_.Quoted(event.signature))) return false;
                break;
            case dex::DBG_END_LOCAL:
                if (!state_.Append(".end local v") || !state_.Number(event.reg)) return false;
                break;
            case dex::DBG_RESTART_LOCAL:
                if (!state_.Append(".restart local v") || !state_.Number(event.reg)) return false;
                break;
            case dex::DBG_SET_PROLOGUE_END: if (!state_.Append(".prologue")) return false; break;
            case dex::DBG_SET_EPILOGUE_BEGIN: if (!state_.Append(".epilogue")) return false; break;
            case dex::DBG_SET_FILE:
                if (!state_.Append(".source")) return false;
                if (event.name != dex::kNoIndex && (!state_.Append(" ") || !dex_.Quoted(event.name))) return false;
                break;
            default: return state_.Fail(SmaliError::InternalError);
        }
        if (!state_.Append("\n")) return false;
    }
    return true;
}

} // namespace dexkit::smali
