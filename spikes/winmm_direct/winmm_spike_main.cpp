#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdlib>
#include <cwctype>
#include <iostream>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

using namespace std::chrono_literals;

namespace {

struct WinmmIdentity {
    std::wstring name;
    WORD manufacturer_id{};
    WORD product_id{};
    DWORD driver_version{};

    bool operator==(const WinmmIdentity&) const = default;
};

enum class ResolveStatus { resolved, missing, ambiguous };

ResolveStatus resolve_exact(const WinmmIdentity& persisted,
                            const std::vector<WinmmIdentity>& available) {
    const auto count = std::count(available.begin(), available.end(), persisted);
    if (count == 1) return ResolveStatus::resolved;
    return count == 0 ? ResolveStatus::missing : ResolveStatus::ambiguous;
}

bool run_identity_tests() {
    const WinmmIdentity persisted{L"near duplicate", 1, 2, 0x0102};
    const WinmmIdentity different_driver{L"near duplicate", 1, 2, 0x0103};
    const WinmmIdentity different_pid{L"near duplicate", 1, 3, 0x0102};
    const std::vector<WinmmIdentity> initial{different_driver, persisted, different_pid};
    if (resolve_exact(persisted, initial) != ResolveStatus::resolved) return false;
    const std::vector<WinmmIdentity> reordered{different_pid, persisted, different_driver};
    if (resolve_exact(persisted, reordered) != ResolveStatus::resolved) return false;
    if (resolve_exact(persisted, {different_driver, different_pid}) != ResolveStatus::missing) return false;
    if (resolve_exact(persisted, {persisted}) != ResolveStatus::resolved) return false;
    return resolve_exact(persisted, {persisted, persisted}) == ResolveStatus::ambiguous;
}

std::string utf8(const std::wstring& value) {
    if (value.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                                         nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                        result.data(), size, nullptr, nullptr);
    return result;
}

std::string json_escape(const std::string& value) {
    std::string result;
    for (const char ch : value) {
        if (ch == '\\') result += "\\\\";
        else if (ch == '"') result += "\\\"";
        else result += ch;
    }
    return result;
}

std::wstring lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](const wchar_t ch) { return static_cast<wchar_t>(std::towlower(ch)); });
    return value;
}

struct PortInfo {
    UINT index{};
    WinmmIdentity identity;
};

std::vector<PortInfo> enumerate_inputs(const bool emit = true) {
    std::vector<PortInfo> ports;
    for (UINT index = 0; index < midiInGetNumDevs(); ++index) {
        MIDIINCAPSW caps{};
        const auto result = midiInGetDevCapsW(index, &caps, sizeof(caps));
        if (result != MMSYSERR_NOERROR) continue;
        PortInfo info{index, {caps.szPname, caps.wMid, caps.wPid, caps.vDriverVersion}};
        ports.push_back(info);
        if (emit) {
            std::cout << "{\"event\":\"winmm_input\",\"index_hint\":" << index
                      << ",\"name\":\"" << json_escape(utf8(info.identity.name))
                      << "\",\"wMid\":" << caps.wMid << ",\"wPid\":" << caps.wPid
                      << ",\"driver_version\":" << caps.vDriverVersion << "}" << std::endl;
        }
    }
    return ports;
}

std::vector<PortInfo> enumerate_outputs(const bool emit = true) {
    std::vector<PortInfo> ports;
    for (UINT index = 0; index < midiOutGetNumDevs(); ++index) {
        MIDIOUTCAPSW caps{};
        const auto result = midiOutGetDevCapsW(index, &caps, sizeof(caps));
        if (result != MMSYSERR_NOERROR) continue;
        PortInfo info{index, {caps.szPname, caps.wMid, caps.wPid, caps.vDriverVersion}};
        ports.push_back(info);
        if (emit) {
            std::cout << "{\"event\":\"winmm_output\",\"index_hint\":" << index
                      << ",\"name\":\"" << json_escape(utf8(info.identity.name))
                      << "\",\"wMid\":" << caps.wMid << ",\"wPid\":" << caps.wPid
                      << ",\"driver_version\":" << caps.vDriverVersion << "}" << std::endl;
        }
    }
    return ports;
}

struct SafePair {
    PortInfo input;
    PortInfo output;
};

std::optional<SafePair> find_exact_temporary_pair(const std::vector<PortInfo>& inputs,
                                                  const std::vector<PortInfo>& outputs,
                                                  const std::wstring& expected_input_name,
                                                  const std::wstring& expected_output_name) {
    std::vector<PortInfo> matching_inputs;
    std::vector<PortInfo> matching_outputs;
    std::copy_if(inputs.begin(), inputs.end(), std::back_inserter(matching_inputs),
                 [&](const PortInfo& port) { return port.identity.name == expected_input_name; });
    std::copy_if(outputs.begin(), outputs.end(), std::back_inserter(matching_outputs),
                 [&](const PortInfo& port) { return port.identity.name == expected_output_name; });
    if (matching_inputs.size() != 1 || matching_outputs.size() != 1) return std::nullopt;
    return SafePair{matching_inputs.front(), matching_outputs.front()};
}

struct InputBuffer {
    std::vector<char> storage;
    MIDIHDR header{};
    bool prepared{false};
};

struct InputContext {
    HMIDIIN handle{};
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<std::unique_ptr<InputBuffer>> buffers;
    std::vector<std::uint8_t> long_bytes;
    std::vector<DWORD> short_messages;
    std::atomic<bool> accepting{false};
    std::atomic<bool> closed{false};
    std::atomic<unsigned> callbacks_during_teardown{0};
    std::atomic<unsigned> callbacks_after_stop{0};
    std::atomic<unsigned> long_completions{0};
    std::atomic<unsigned> requeue_failures{0};
};

void CALLBACK input_callback(HMIDIIN, UINT message, DWORD_PTR instance,
                             DWORD_PTR param1, DWORD_PTR) {
    auto* context = reinterpret_cast<InputContext*>(instance);
    if (!context) return;
    if (!context->accepting.load(std::memory_order_acquire)) {
        if (message == MIM_DATA || message == MIM_LONGDATA) {
            if (context->closed.load(std::memory_order_acquire)) ++context->callbacks_after_stop;
            else ++context->callbacks_during_teardown;
        }
        return;
    }
    if (message == MIM_DATA) {
        {
            std::scoped_lock lock(context->mutex);
            context->short_messages.push_back(static_cast<DWORD>(param1));
        }
        context->changed.notify_all();
    } else if (message == MIM_LONGDATA) {
        auto* header = reinterpret_cast<MIDIHDR*>(param1);
        if (!header) return;
        {
            std::scoped_lock lock(context->mutex);
            const auto* begin = reinterpret_cast<const std::uint8_t*>(header->lpData);
            context->long_bytes.insert(context->long_bytes.end(), begin,
                                       begin + header->dwBytesRecorded);
            ++context->long_completions;
        }
        context->changed.notify_all();
        header->dwBytesRecorded = 0;
        if (context->accepting.load(std::memory_order_acquire)) {
            if (midiInAddBuffer(context->handle, header, sizeof(MIDIHDR)) != MMSYSERR_NOERROR) {
                ++context->requeue_failures;
            }
        }
    }
}

struct OutputContext {
    std::mutex mutex;
    std::condition_variable changed;
    MIDIHDR* completed_header{};
};

void CALLBACK output_callback(HMIDIOUT, UINT message, DWORD_PTR instance,
                              DWORD_PTR param1, DWORD_PTR) {
    if (message != MOM_DONE) return;
    auto* context = reinterpret_cast<OutputContext*>(instance);
    if (!context) return;
    {
        std::scoped_lock lock(context->mutex);
        context->completed_header = reinterpret_cast<MIDIHDR*>(param1);
    }
    context->changed.notify_all();
}

std::string mm_error(const MMRESULT result) {
    wchar_t text[MAXERRORLENGTH]{};
    if (midiInGetErrorTextW(result, text, MAXERRORLENGTH) == MMSYSERR_NOERROR) return utf8(text);
    return "MMRESULT " + std::to_string(result);
}

void require_mm(const MMRESULT result, const char* operation) {
    if (result != MMSYSERR_NOERROR) {
        throw std::runtime_error(std::string(operation) + ": " + mm_error(result));
    }
}

bool exercise_winmm_cycle(const SafePair& pair, const std::wstring& expected_input_name,
                          const std::wstring& expected_output_name, const unsigned cycle,
                          const bool send_evidence) {
    InputContext input;
    OutputContext output;
    HMIDIOUT output_handle{};
    bool ok = false;
    try {
        if (pair.input.identity.name != expected_input_name ||
            pair.output.identity.name != expected_output_name ||
            lower(pair.input.identity.name).find(L"taureon s1 wms") != 0 ||
            lower(pair.output.identity.name).find(L"taureon s1 wms") != 0) {
            throw std::runtime_error("selected WinMM ports are not the exact temporary WMS loopback routes");
        }
        require_mm(midiInOpen(&input.handle, pair.input.index,
                              reinterpret_cast<DWORD_PTR>(&input_callback),
                              reinterpret_cast<DWORD_PTR>(&input), CALLBACK_FUNCTION),
                   "midiInOpen");
        for (int index = 0; index < 2; ++index) {
            auto buffer = std::make_unique<InputBuffer>();
            buffer->storage.resize(1024);
            buffer->header.lpData = buffer->storage.data();
            buffer->header.dwBufferLength = static_cast<DWORD>(buffer->storage.size());
            require_mm(midiInPrepareHeader(input.handle, &buffer->header, sizeof(MIDIHDR)),
                       "midiInPrepareHeader");
            buffer->prepared = true;
            require_mm(midiInAddBuffer(input.handle, &buffer->header, sizeof(MIDIHDR)),
                       "midiInAddBuffer");
            input.buffers.push_back(std::move(buffer));
        }
        input.accepting.store(true, std::memory_order_release);
        require_mm(midiInStart(input.handle), "midiInStart");
        require_mm(midiOutOpen(&output_handle, pair.output.index,
                               reinterpret_cast<DWORD_PTR>(&output_callback),
                               reinterpret_cast<DWORD_PTR>(&output), CALLBACK_FUNCTION),
                   "midiOutOpen");

        bool receive_ok = true;
        if (send_evidence) {
            const DWORD note_on = 0x00013c90;
            require_mm(midiOutShortMsg(output_handle, note_on), "midiOutShortMsg");

            std::vector<std::uint8_t> sysex{0xf0, 0x7d, 1, 2, 3, 4, 5, 6, 7, 0xf7};
            MIDIHDR output_header{};
            output_header.lpData = reinterpret_cast<LPSTR>(sysex.data());
            output_header.dwBufferLength = static_cast<DWORD>(sysex.size());
            require_mm(midiOutPrepareHeader(output_handle, &output_header, sizeof(MIDIHDR)),
                       "midiOutPrepareHeader");
            require_mm(midiOutLongMsg(output_handle, &output_header, sizeof(MIDIHDR)),
                       "midiOutLongMsg");
            {
                std::unique_lock lock(output.mutex);
                receive_ok = output.changed.wait_for(lock, 5s, [&] {
                    return output.completed_header == &output_header;
                });
            }
            require_mm(midiOutUnprepareHeader(output_handle, &output_header, sizeof(MIDIHDR)),
                       "midiOutUnprepareHeader");
            bool short_integrity = false;
            bool long_integrity = false;
            {
                std::unique_lock lock(input.mutex);
                receive_ok = input.changed.wait_for(lock, 5s, [&] {
                    return !input.short_messages.empty() && input.long_bytes.size() >= sysex.size();
                }) && receive_ok;
            }
            {
                std::scoped_lock lock(input.mutex);
                short_integrity = !input.short_messages.empty() &&
                                  (input.short_messages.front() & 0x00ffffffU) == note_on;
                long_integrity = input.long_bytes == sysex;
                receive_ok = receive_ok && short_integrity && long_integrity;
            }
            std::cout << "{\"event\":\"winmm_messages\",\"short_integrity\":"
                      << (short_integrity ? "true" : "false")
                      << ",\"long_integrity\":" << (long_integrity ? "true" : "false")
                      << ",\"sysex_bytes\":" << sysex.size()
                      << ",\"output_header_completed\":"
                      << (output.completed_header == &output_header ? "true" : "false")
                      << ",\"output_header_unprepared\":true,\"long_completions\":"
                      << input.long_completions.load() << "}"
                      << std::endl;
        }

        require_mm(midiOutReset(output_handle), "midiOutReset");
        require_mm(midiOutClose(output_handle), "midiOutClose");
        output_handle = nullptr;
        input.accepting.store(false, std::memory_order_release);
        require_mm(midiInStop(input.handle), "midiInStop");
        require_mm(midiInReset(input.handle), "midiInReset");
        for (auto& buffer : input.buffers) {
            require_mm(midiInUnprepareHeader(input.handle, &buffer->header, sizeof(MIDIHDR)),
                       "midiInUnprepareHeader");
            buffer->prepared = false;
        }
        require_mm(midiInClose(input.handle), "midiInClose");
        input.handle = nullptr;
        input.closed.store(true, std::memory_order_release);
        std::this_thread::sleep_for(20ms);
        ok = receive_ok && input.callbacks_after_stop.load() == 0 &&
             input.requeue_failures.load() == 0;
    } catch (const std::exception& error) {
        std::cerr << "{\"event\":\"winmm_error\",\"cycle\":" << cycle
                  << ",\"message\":\"" << json_escape(error.what()) << "\"}" << std::endl;
    }

    if (output_handle) {
        midiOutReset(output_handle);
        midiOutClose(output_handle);
    }
    if (input.handle) {
        input.accepting.store(false, std::memory_order_release);
        midiInStop(input.handle);
        midiInReset(input.handle);
        for (auto& buffer : input.buffers) {
            if (buffer->prepared) midiInUnprepareHeader(input.handle, &buffer->header, sizeof(MIDIHDR));
        }
        midiInClose(input.handle);
    }
    std::cout << "{\"event\":\"winmm_lifecycle\",\"cycle\":" << cycle
              << ",\"callbacks_during_teardown\":" << input.callbacks_during_teardown.load()
              << ",\"callbacks_after_close\":" << input.callbacks_after_stop.load()
              << ",\"requeue_failures\":" << input.requeue_failures.load()
              << ",\"ok\":" << (ok ? "true" : "false") << "}" << std::endl;
    return ok;
}

} // namespace

int main(int argc, char** argv) {
    const auto to_wide = [](const std::string& value) {
        if (value.empty()) return std::wstring{};
        const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                              static_cast<int>(value.size()), nullptr, 0);
        if (count <= 0) throw std::runtime_error("invalid UTF-8 command-line endpoint name");
        std::wstring result(static_cast<std::size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), result.data(), count);
        return result;
    };

    const bool identity_ok = run_identity_tests();
    const std::string command = argc > 1 ? argv[1] : "";
    if (command == "--enumerate-only") {
        const auto inputs = enumerate_inputs();
        const auto outputs = enumerate_outputs();
        std::cout << "{\"event\":\"winmm_enumeration_summary\",\"inputs\":"
                  << inputs.size() << ",\"outputs\":" << outputs.size() << "}" << std::endl;
        return EXIT_SUCCESS;
    }
    if (command == "--identity-only") {
        std::cout << "{\"event\":\"winmm_identity_logic\",\"index_is_hint_only\":true,"
                     "\"no_fuzzy_rebind\":true,\"missing_visible\":true,"
                     "\"ambiguous_visible\":true,\"passed\":"
                  << (identity_ok ? "true" : "false") << "}" << std::endl;
        return identity_ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (command == "--inspect-pair" && argc == 4) {
        const auto input_name = to_wide(argv[2]);
        const auto output_name = to_wide(argv[3]);
        const auto pair = find_exact_temporary_pair(enumerate_inputs(false), enumerate_outputs(false),
                                                    input_name, output_name);
        if (!pair) {
            std::cout << "{\"event\":\"winmm_pair_inspection\",\"status\":\"missing_or_ambiguous\"}"
                      << std::endl;
            return EXIT_FAILURE;
        }
        std::cout << "{\"event\":\"winmm_pair_inspection\",\"status\":\"exact_unique\","
                  << "\"input_name\":\"" << json_escape(utf8(pair->input.identity.name))
                  << "\",\"input_wMid\":" << pair->input.identity.manufacturer_id
                  << ",\"input_wPid\":" << pair->input.identity.product_id
                  << ",\"input_driver\":" << pair->input.identity.driver_version
                  << ",\"output_name\":\"" << json_escape(utf8(pair->output.identity.name))
                  << "\",\"output_wMid\":" << pair->output.identity.manufacturer_id
                  << ",\"output_wPid\":" << pair->output.identity.product_id
                  << ",\"output_driver\":" << pair->output.identity.driver_version
                  << ",\"input_index_hint\":" << pair->input.index
                  << ",\"output_index_hint\":" << pair->output.index << "}" << std::endl;
        return EXIT_SUCCESS;
    }
    if (command == "--assert-absent" && argc == 4) {
        const auto name_a = to_wide(argv[2]);
        const auto name_b = to_wide(argv[3]);
        const auto inputs = enumerate_inputs(false);
        const auto outputs = enumerate_outputs(false);
        const auto has_name = [&](const PortInfo& port) {
            return port.identity.name == name_a || port.identity.name == name_b;
        };
        const bool absent = std::none_of(inputs.begin(), inputs.end(), has_name) &&
                            std::none_of(outputs.begin(), outputs.end(), has_name);
        std::cout << "{\"event\":\"winmm_pair_absence\",\"absent\":"
                  << (absent ? "true" : "false") << "}" << std::endl;
        return absent ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (command != "--wms-loopback" || argc != 5) {
        std::cerr << "usage: taureon_winmm_spike --wms-loopback <input-B-name> "
                     "<output-A-name> <cycles>"
                  << std::endl;
        return EXIT_FAILURE;
    }

    const auto expected_input_name = to_wide(argv[2]);
    const auto expected_output_name = to_wide(argv[3]);
    const auto cycles = static_cast<unsigned>(std::stoul(argv[4]));
    const auto inputs = enumerate_inputs();
    const auto outputs = enumerate_outputs();
    const auto safe_pair = find_exact_temporary_pair(inputs, outputs, expected_input_name,
                                                     expected_output_name);
    if (!safe_pair) {
        std::cerr << "{\"event\":\"stop_ask\",\"reason\":"
                     "\"temporary WMS loopback pair is missing or ambiguous in WinMM\"}"
                  << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "{\"event\":\"safe_test_pair\",\"substrate\":\"WMS native loopback\","
              << "\"input_name\":\"" << json_escape(utf8(safe_pair->input.identity.name))
              << "\",\"output_name\":\"" << json_escape(utf8(safe_pair->output.identity.name))
              << "\",\"input_index_hint\":" << safe_pair->input.index
              << ",\"output_index_hint\":" << safe_pair->output.index
              << ",\"exact_names_verified\":true}" << std::endl;

    bool all_ok = identity_ok && cycles > 0;
    DWORD handles_after_first_cycle = 0;
    for (unsigned cycle = 1; cycle <= cycles; ++cycle) {
        all_ok = exercise_winmm_cycle(*safe_pair, expected_input_name, expected_output_name,
                                      cycle, cycle == 1) && all_ok;
        if (cycle == 1 && !GetProcessHandleCount(GetCurrentProcess(), &handles_after_first_cycle)) {
            all_ok = false;
        }
    }
    DWORD handles_after_final_cycle = 0;
    const bool handle_query_ok = GetProcessHandleCount(GetCurrentProcess(), &handles_after_final_cycle) != 0;
    const bool handle_growth_ok = handle_query_ok &&
                                  handles_after_final_cycle <= handles_after_first_cycle;
    std::cout << "{\"event\":\"winmm_handle_stability\",\"after_cycle_1\":"
              << handles_after_first_cycle << ",\"after_final_cycle\":"
              << handles_after_final_cycle << ",\"growth\":"
              << static_cast<long long>(handles_after_final_cycle) -
                     static_cast<long long>(handles_after_first_cycle)
              << ",\"stable\":" << (handle_growth_ok ? "true" : "false") << "}"
              << std::endl;
    all_ok = all_ok && handle_growth_ok;

    std::vector<WinmmIdentity> input_identities;
    std::vector<WinmmIdentity> output_identities;
    for (const auto& port : enumerate_inputs(false)) input_identities.push_back(port.identity);
    for (const auto& port : enumerate_outputs(false)) output_identities.push_back(port.identity);
    const auto input_status = resolve_exact(safe_pair->input.identity, input_identities);
    const auto output_status = resolve_exact(safe_pair->output.identity, output_identities);
    const bool persisted_ok = input_status == ResolveStatus::resolved &&
                              output_status == ResolveStatus::resolved;
    std::cout << "{\"event\":\"winmm_persisted_resolution\",\"input_status\":\""
              << (input_status == ResolveStatus::resolved ? "resolved" :
                  input_status == ResolveStatus::missing ? "missing" : "ambiguous")
              << "\",\"output_status\":\""
              << (output_status == ResolveStatus::resolved ? "resolved" :
                  output_status == ResolveStatus::missing ? "missing" : "ambiguous")
              << "\",\"silent_fallback\":false}" << std::endl;
    all_ok = all_ok && persisted_ok;
    std::cout << "{\"event\":\"winmm_summary\",\"cycles\":" << cycles
              << ",\"pass\":" << (all_ok ? "true" : "false") << "}" << std::endl;
    return all_ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
