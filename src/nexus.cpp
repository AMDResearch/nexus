
#define __HIP_PLATFORM_AMD__

#include "nexus.hpp"
#include <fmt/core.h>
#include "log.hpp"

#include <cxxabi.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <hip/hip_runtime.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <exception>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>

#include <hip/hip_runtime.h>

namespace maestro {

std::mutex nexus::mutex_{};
std::shared_mutex nexus::stop_mutex_{};
nexus* nexus::singleton_{nullptr};

static std::string read_line_from_file(const std::string& filename, size_t line_number) {
  std::ifstream file(filename);
  if (!file) {
    LOG_ERROR("Cannot open file {}", filename);
    return "";
  }

  std::string line;
  size_t current_line = 0;

  while (std::getline(file, line)) {
    if (current_line == line_number) {
      return line;
    }
    current_line++;
  }
  LOG_ERROR("Error: Line number {} not found in file {}", line_number, filename);
  return "";
}

nexus::nexus(HsaApiTable* table,
             uint64_t runtime_version,
             uint64_t failed_tool_count,
             const char* const* failed_tool_names)
    : api_table_{table} {
  LOG_DETAIL("Saving current APIs.");
  save_hsa_api();
  LOG_DETAIL("Hooking new APIs.");
  hook_api();
  LOG_DETAIL("Discovering agents.");
  discover_agents();

  for (const auto& pair : agents_names_) {
    LOG_DETAIL("Agent Handle: 0x{:x} , Name: {}", pair.first.handle, pair.second);
  }

  // Create the FIFO
  static const char* pipe_name = std::getenv("NEXUS_PIPE_NAME");
  if (pipe_name) {
    if (mkfifo(pipe_name, 0666) == -1 && errno != EEXIST) {
      LOG_DETAIL("mkfifo failed");
    }
  } else {
    LOG_INFO(
        "NEXUS_PIPE_NAME is not set. Set it to communicate with driver "
        "script.");
  }

  HsaAgent::get_all_agents(agents_);
  for (const auto& agent : agents_) {
    agent.print_info();
  }

  HsaAgent gpu_agent;
  bool gpu_agent_exist = HsaAgent::find_first_gpu_agent(agents_, gpu_agent);
  if (!gpu_agent_exist) {
    LOG_ERROR("No GPU Agent Found");
    std::terminate();
  }
  gpu_agent_ = gpu_agent;
  kdb_ = std::make_unique<kernelDB::kernelDB>(gpu_agent.agent);

  if (kdb_) {
    std::vector<std::string> kernels;
    std::vector<uint32_t> lines;
    kdb_->getKernels(kernels);
    LOG_INFO("Found {} kernels", kernels.size());
    for (std::size_t kernel_idx = 0; kernel_idx < kernels.size(); kernel_idx++) {
      const auto kernel = kernels[kernel_idx];
      std::vector<uint32_t> lines;
      kdb_->getKernelLines(kernel, lines);
      const auto kernel_filename = kdb_->getFileName(kernel, kernel_idx);
      LOG_INFO("Kernel: {}", kernel);
      LOG_INFO("Number of lines: {}", lines.size());
      for (auto& line : lines) {
        try {
          const auto& inst = kdb_->getInstructionsForLine(kernel, line);
          for (size_t idx = 0; idx < inst.size(); idx++) {
            LOG_INFO("{}:{} -> {}", inst[idx].file_name_, line, inst[idx].disassembly_);
          }
        } catch (std::runtime_error e) {
          LOG_ERROR("Error: {}", e.what());
        }
      }
    }
  }
}

static void* memcpy_d2h(const void* device_ptr,
                        size_t size,
                        const std::vector<HsaAgent>& agents_) {
  // Find a CPU agent with a fine-grained memory region
  for (const auto& agent : agents_) {
    if (!agent.is_gpu) {  // Find a CPU agent
      for (const auto& region : agent.memory_regions) {
        if (region.is_global) {  // Fine-grained memory for CPU
          void* host_ptr = nullptr;
          hsa_status_t status = hsa_memory_allocate(region.region, size, &host_ptr);

          LOG_DETAIL("Allocated CPU pointer {} ({} bytes).", host_ptr, size);
          if (status != HSA_STATUS_SUCCESS || host_ptr == nullptr) {
            LOG_DETAIL("Failed to allocate fine-grained host memory of size {}", size);
            return nullptr;
          }

          // Copy memory from device to host
          LOG_DETAIL("D2H copying to {} from ({} bytes).", host_ptr, device_ptr, size);

          status = hsa_memory_copy(host_ptr, device_ptr, size);
          if (status != HSA_STATUS_SUCCESS) {
            LOG_DETAIL("Failed to copy device memory to host");
            hsa_amd_memory_pool_free(host_ptr);
            return nullptr;
          }

          return host_ptr;
        }
      }
    }
  }

  LOG_DETAIL("No suitable CPU agent with fine-grained memory found.");
  return nullptr;
}

template <typename T, typename Func, std::size_t... Is>
inline void for_each_field_impl(const T& obj, Func func, std::index_sequence<Is...>) {
  (func(std::get<Is>(obj->as_tuple())), ...);
}
template <typename T, typename Func>
inline void for_each_field(const T& obj, Func func) {
  constexpr std::size_t N = std::tuple_size_v<decltype(obj->as_tuple())>;
  for_each_field_impl(obj, func, std::make_index_sequence<N>{});
}

void printHipIpcMemHandle(const hipIpcMemHandle_t& handle, const std::string& message) {
  const unsigned char* data = reinterpret_cast<const unsigned char*>(&handle);
  LOG_DETAIL("{} hipIpcMemHandle_t contents:", message);

  std::ostringstream buffer;
  buffer << std::hex << std::setfill('0');
  for (size_t i = 0; i < sizeof(hipIpcMemHandle_t); ++i) {
    buffer << std::setw(2) << static_cast<int>(data[i]) << " ";
    if ((i + 1) % 16 == 0 || i + 1 == sizeof(hipIpcMemHandle_t)) {
      LOG_DETAIL("{}", buffer.str());
      buffer.str("");
      buffer.clear();
    }
  }
}

void writeIpcHandleToFile(const hipIpcMemHandle_t& handle, size_t ptr_size) {
  static const char* file_name = std::getenv("NEXUS_IPC_OUTPUT_FILE");
  if (!file_name) {
    LOG_ERROR("Set NEXUS_IPC_OUTPUT_FILE to communicate with driver script.");
    std::terminate();
  }
  std::ofstream file(file_name, std::ios::binary | std::ios::app);
  if (!file) {
    LOG_ERROR("Failed to open file for writing:  {}", file_name);
    return;
  }

  file << "BEGIN\n";
  file.write(reinterpret_cast<const char*>(&handle), sizeof(handle));
  file.write(reinterpret_cast<const char*>(&ptr_size), sizeof(ptr_size));
  file << "END\n";
  file.flush();
  file.close();

  LOG_DETAIL("Appended IPC handle and size ({} bytes) to file: {}", ptr_size, file_name);
}
void nexus::send_message_and_wait(void* args) {}

void nexus::discover_agents() {
  auto agent_callback = [](hsa_agent_t agent, void* data) -> hsa_status_t {
    auto* agents_map =
        static_cast<std::map<hsa_agent_t, std::string, hsa_agent_compare>*>(data);

    char name[64] = {0};
    if (hsa_agent_get_info(agent, HSA_AGENT_INFO_NAME, name) != HSA_STATUS_SUCCESS) {
      return HSA_STATUS_ERROR;
    }
    (*agents_map)[agent] = std::string(name);
    return HSA_STATUS_SUCCESS;
  };

  hsa_core_call(this, hsa_iterate_agents, agent_callback, &agents_names_);
}

std::string demangle_name(const char* mangled_name) {
  int status = 0;
  std::unique_ptr<char, void (*)(void*)> result(
      abi::__cxa_demangle(mangled_name, nullptr, nullptr, &status), std::free);
  return (status == 0) ? result.get() : mangled_name;
}

std::string nexus::get_kernel_name(const std::uint64_t kernel_object) {
  auto handle_find_result = handles_symbols_.find(kernel_object);
  if (handle_find_result == handles_symbols_.end()) {
    return "Object not found.";
  }
  auto symbol_find_result = symbols_names_.find(handle_find_result->second);
  if (handle_find_result == handles_symbols_.end()) {
    return "Symbol not found.";
  }
  auto demangled_name = demangle_name(symbol_find_result->second.c_str());

  size_t clone_suffix_pos = demangled_name.find(" [clone");
  if (clone_suffix_pos != std::string::npos) {
    demangled_name = demangled_name.substr(0, clone_suffix_pos);
  }

  return demangled_name;
}

std::string nexus::packet_to_text(const hsa_ext_amd_aql_pm4_packet_t* packet) {
  std::ostringstream buff;
  uint32_t type = get_header_type(packet);

  switch (type) {
    case HSA_PACKET_TYPE_VENDOR_SPECIFIC: {
      buff << "HSA_PACKET_TYPE_VENDOR_SPECIFIC";
      break;
    }
    case HSA_PACKET_TYPE_INVALID: {
      buff << "HSA_PACKET_TYPE_INVALID";
      break;
    }
    case HSA_PACKET_TYPE_KERNEL_DISPATCH: {
      const hsa_kernel_dispatch_packet_t* disp =
          reinterpret_cast<const hsa_kernel_dispatch_packet_t*>(packet);
      uint32_t scope = get_header_release_scope(disp);
      const auto kernel_name = get_kernel_name(disp->kernel_object);
      static const char* kernel_to_trace = std::getenv("KERNEL_TO_TRACE");

      if (kernel_to_trace != nullptr && kernel_name.starts_with(kernel_to_trace)) {
        buff << ("\nTracing the kernel\n");
      }

      buff << "Dispatch Packet\n"
           << fmt::format("\tKernel name: {}\n", kernel_name) << "\tRelease Scope: ";

      if (scope & HSA_FENCE_SCOPE_AGENT) {
        buff << "HSA_FENCE_SCOPE_AGENT";
      } else if (scope & HSA_FENCE_SCOPE_SYSTEM) {
        buff << "HSA_FENCE_SCOPE_SYSTEM";
      } else {
        buff << fmt::format("0x{:x}", scope);
      }

      buff << "\n\tAcquire Scope: ";
      scope = get_header_acquire_scope(disp);
      if (scope & HSA_FENCE_SCOPE_AGENT) {
        buff << "HSA_FENCE_SCOPE_AGENT";
      } else if (scope & HSA_FENCE_SCOPE_SYSTEM) {
        buff << "HSA_FENCE_SCOPE_SYSTEM";
      } else {
        buff << "Unkown Scope";
      }

      buff << fmt::format("\n\tsetup: 0x{:x}\n", disp->setup);
      buff << fmt::format("\tworkgroup_size_x: 0x{:x}\n", disp->workgroup_size_x);
      buff << fmt::format("\tworkgroup_size_y: 0x{:x}\n", disp->workgroup_size_y);
      buff << fmt::format("\tworkgroup_size_z: 0x{:x}\n", disp->workgroup_size_z);
      buff << fmt::format("\tgrid_size_x: 0x{:x}\n", disp->grid_size_x);
      buff << fmt::format("\tgrid_size_y: 0x{:x}\n", disp->grid_size_y);
      buff << fmt::format("\tgrid_size_z: 0x{:x}\n", disp->grid_size_z);
      buff << fmt::format("\tprivate_segment_size: 0x{:x}\n", disp->private_segment_size);
      buff << fmt::format("\tgroup_segment_size: 0x{:x}\n", disp->group_segment_size);
      buff << fmt::format("\tkernel_object: 0x{:x}\n", disp->kernel_object);
      buff << fmt::format("\tkernarg_address: 0x{:x}\n",
                          reinterpret_cast<uintptr_t>(disp->kernarg_address));
      buff << fmt::format("\tcompletion_signal: 0x{:x}", disp->completion_signal.handle);
      break;
    }
    case HSA_PACKET_TYPE_BARRIER_AND: {
      buff << "HSA_PACKET_TYPE_BARRIER_AND";
      break;
    }
    case HSA_PACKET_TYPE_AGENT_DISPATCH: {
      buff << "HSA_PACKET_TYPE_AGENT_DISPATCH";
      break;
    }
    case HSA_PACKET_TYPE_BARRIER_OR: {
      buff << "HSA_PACKET_TYPE_BARRIER_OR";
      break;
    }
    default: {
      buff << "Unsupported packet type";
      break;
    }
  }

  return buff.str();
}

std::optional<std::string> nexus::is_traceable_packet(
    const hsa_ext_amd_aql_pm4_packet_t* packet) {
  uint32_t type = get_header_type(packet);
  switch (type) {
    case HSA_PACKET_TYPE_KERNEL_DISPATCH: {
      const hsa_kernel_dispatch_packet_t* disp =
          reinterpret_cast<const hsa_kernel_dispatch_packet_t*>(packet);
      uint32_t scope = get_header_release_scope(disp);
      const auto kernel_name = get_kernel_name(disp->kernel_object);
      static const char* kernel_to_trace = std::getenv("KERNEL_TO_TRACE");

      if (kernel_to_trace == nullptr) {
        return kernel_name;
      } else if (kernel_name.contains(kernel_to_trace)) {
        LOG_INFO("Found the target kernel {}", kernel_name);
        return kernel_name;
      }
    }
  }
  return {};
}

nexus* nexus::get_instance(HsaApiTable* table,
                           uint64_t runtime_version,
                           uint64_t failed_tool_count,
                           const char* const* failed_tool_names) {
  const std::lock_guard<std::mutex> lock(mutex_);
  if (!singleton_) {
    if (table != NULL) {
      singleton_ =
          new nexus(table, runtime_version, failed_tool_count, failed_tool_names);
    } else {
    }
  }
  return singleton_;
}

nexus::~nexus() {
  delete rocr_api_table_.core_;
  delete rocr_api_table_.amd_ext_;
  delete rocr_api_table_.finalizer_ext_;
  delete rocr_api_table_.image_ext_;
}

hsa_status_t nexus::hsa_code_object_reader_create_from_file(
    hsa_file_t file,
    hsa_code_object_reader_t* code_object_reader) {
  LOG_DETAIL("Creating a code object reader from file {}", file);
  auto instance = get_instance();
  auto result = hsa_core_call(
      instance, hsa_code_object_reader_create_from_file, file, code_object_reader);
  if (result != HSA_STATUS_SUCCESS) {
    LOG_ERROR("Failed to create a code object reader from file {}", file);
  }
  return result;
}

static std::optional<std::string> find_mmap_file_from_ptr(const void* ptr) {
  std::ifstream maps("/proc/self/maps");
  std::string line;

  const uintptr_t address = reinterpret_cast<uintptr_t>(ptr);

  while (std::getline(maps, line)) {
    std::istringstream iss(line);
    std::string addr_range;
    iss >> addr_range;

    const size_t dash_pos = addr_range.find('-');
    if (dash_pos == std::string::npos)
      continue;

    const uintptr_t start = std::stoull(addr_range.substr(0, dash_pos), nullptr, 16);
    const uintptr_t end = std::stoull(addr_range.substr(dash_pos + 1), nullptr, 16);

    if (address >= start && address < end) {
      std::string unused, path;
      for (int i = 0; i < 4 && iss; ++i)
        iss >> unused;

      std::getline(iss, path);
      path.erase(0, path.find_first_not_of(" \t"));

      return path.empty() ? "[anonymous mapping]" : path;
    }
  }

  return {};
}

hsa_status_t nexus::hsa_code_object_reader_create_from_memory(
    const void* code_object,
    size_t size,
    hsa_code_object_reader_t* code_object_reader) {
  const auto filename = find_mmap_file_from_ptr(code_object);

  LOG_DETAIL("Creating a code object reader from memory {} ({} bytes) (filename: {})",
             code_object,
             size,
             filename.value_or("unknown"));

  auto instance = get_instance();
  auto result = hsa_core_call(instance,
                              hsa_code_object_reader_create_from_memory,
                              code_object,
                              size,
                              code_object_reader);
  if (result != HSA_STATUS_SUCCESS) {
    LOG_ERROR("Failed to create a code object reader from memory {} ({} bytes)",
              code_object,
              size);
  }

  if (instance->kdb_ && filename.has_value()) {
    instance->kdb_->addFile(filename.value(), instance->gpu_agent_.agent, "");
  }
  return result;
}

hsa_status_t nexus::hsa_executable_get_symbol_by_name(hsa_executable_t executable,
                                                      const char* symbol_name,
                                                      const hsa_agent_t* agent,
                                                      hsa_executable_symbol_t* symbol) {
  LOG_DETAIL("Looking up the kernel {} (demangled: {})",
             symbol_name,
             demangle_name(symbol_name));

  auto instance = get_instance();
  auto result = hsa_core_call(instance,
                              hsa_executable_get_symbol_by_name,
                              executable,
                              symbol_name,
                              agent,
                              symbol);

  {
    std::lock_guard g(mutex_);
    const std::string kernel_name = std::string(symbol_name);
    instance->symbols_names_[*symbol] = kernel_name;
    instance->kernels_executables_[kernel_name] = executable;
  }

  return result;
}

hsa_status_t nexus::hsa_executable_symbol_get_info(
    hsa_executable_symbol_t executable_symbol,
    hsa_executable_symbol_info_t attribute,
    void* value) {
  auto instance = get_instance();
  auto result = hsa_core_call(
      instance, hsa_executable_symbol_get_info, executable_symbol, attribute, value);

  if (result == HSA_STATUS_SUCCESS &&
      attribute == HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT) {
    LOG_DETAIL("Looking up the symbol 0x{:x}", executable_symbol.handle);

    std::lock_guard g(mutex_);
    instance->handles_symbols_[*static_cast<std::uint64_t*>(value)] = executable_symbol;
  }
  return result;
}

void nexus::save_hsa_api() {
  rocr_api_table_.core_ = new CoreApiTable();
  rocr_api_table_.amd_ext_ = new AmdExtTable();
  rocr_api_table_.finalizer_ext_ = new FinalizerExtTable();
  rocr_api_table_.image_ext_ = new ImageExtTable();

  std::memcpy(rocr_api_table_.core_, api_table_->core_, sizeof(CoreApiTable));
  std::memcpy(rocr_api_table_.amd_ext_, api_table_->amd_ext_, sizeof(AmdExtTable));
  std::memcpy(rocr_api_table_.finalizer_ext_,
              api_table_->finalizer_ext_,
              sizeof(FinalizerExtTable));
  std::memcpy(rocr_api_table_.image_ext_, api_table_->image_ext_, sizeof(ImageExtTable));
}
void nexus::restore_hsa_api() {
  copyTables(&rocr_api_table_, api_table_);
}
void nexus::hook_api() {
  api_table_->core_->hsa_queue_create_fn = nexus::hsa_queue_create;
  api_table_->core_->hsa_queue_destroy_fn = nexus::hsa_queue_destroy;

  api_table_->amd_ext_->hsa_amd_memory_pool_allocate_fn =
      nexus::hsa_amd_memory_pool_allocate;
  api_table_->core_->hsa_memory_allocate_fn = nexus::hsa_memory_allocate;

  api_table_->core_->hsa_executable_get_symbol_by_name_fn =
      nexus::hsa_executable_get_symbol_by_name;

  api_table_->core_->hsa_code_object_reader_create_from_file_fn =
      nexus::hsa_code_object_reader_create_from_file;

  api_table_->core_->hsa_code_object_reader_create_from_memory_fn =
      nexus::hsa_code_object_reader_create_from_memory;

  api_table_->core_->hsa_executable_symbol_get_info_fn =
      nexus::hsa_executable_symbol_get_info;
}

hsa_status_t nexus::add_queue(hsa_queue_t* queue, hsa_agent_t agent) {
  std::lock_guard<std::mutex> lock(mm_mutex_);
  auto instance = get_instance();
  auto result =
      hsa_ext_call(instance, hsa_amd_profiling_set_profiler_enabled, queue, true);
  return result;
}

void nexus::on_submit_packet(const void* in_packets,
                             uint64_t count,
                             uint64_t user_que_idx,
                             void* data,
                             hsa_amd_queue_intercept_packet_writer writer) {
  auto instance = get_instance();
  if (instance) {
    hsa_queue_t* queue = reinterpret_cast<hsa_queue_t*>(data);
    instance->write_packets(queue,
                            static_cast<const hsa_ext_amd_aql_pm4_packet_t*>(in_packets),
                            count,
                            writer);
  }
}

void nexus::write_packets(hsa_queue_t* queue,
                          const hsa_ext_amd_aql_pm4_packet_t* packet,
                          uint64_t count,
                          hsa_amd_queue_intercept_packet_writer writer) {
  try {
    LOG_DETAIL("Executing packet: {}", packet_to_text(packet));
    auto instance = get_instance();

    hsa_signal_t new_signal;
    auto status = hsa_core_call(instance, hsa_signal_create, 1, 0, nullptr, &new_signal);

    if (status != HSA_STATUS_SUCCESS) {
      LOG_ERROR("Failed to create signal");
      return;
    }

    writer(packet, count);

    auto kernel_string = is_traceable_packet(packet);
    if (kernel_string.has_value()) {
      const char* env_trace_path = std::getenv("NEXUS_OUTPUT_FILE");

      if (!env_trace_path) {
        LOG_DETAIL(
            "NEXUS_OUTPUT_FILE environment variable not set, skipping kernel trace");
      } else {
        LOG_DETAIL("Dumping the kernels at: {}", env_trace_path);
      }
      if (env_trace_path && kdb_) {
        std::vector<uint32_t> lines;
        kdb_->getKernelLines(kernel_string.value(), lines);
        std::size_t cur_offset{0};

        std::lock_guard<std::mutex> lock(mutex_);

        nlohmann::json line_array = nlohmann::json::array();
        nlohmann::json file_array = nlohmann::json::array();
        nlohmann::json hip_array = nlohmann::json::array();
        nlohmann::json assembly_array = nlohmann::json::array();

        for (std::size_t line_idx = 0; line_idx < lines.size(); line_idx++) {
          const auto& line = lines[line_idx];
          const auto& inst = kdb_->getInstructionsForLine(kernel_string.value(), line);

          for (const auto& instruction_obj : inst) {
            auto instruction = instruction_obj.disassembly_;
            const auto& filename = instruction_obj.file_name_;
            instruction.erase(std::remove(instruction.begin(), instruction.end(), '\t'),
                              instruction.end());

            line_array.push_back(line - 1);
            file_array.push_back(filename);
            hip_array.push_back(read_line_from_file(filename, line - 1));
            assembly_array.push_back(instruction);

            LOG_INFO("{}:{} -> {}", filename, line, instruction);
            cur_offset++;
          }
        }

        const std::string& kernel_name = kernel_string.value();

        json_["kernels"][kernel_name]["lines"] = std::move(line_array);
        json_["kernels"][kernel_name]["files"] = std::move(file_array);
        json_["kernels"][kernel_name]["hip"] = std::move(hip_array);
        json_["kernels"][kernel_name]["assembly"] = std::move(assembly_array);
        json_["kernels"][kernel_name]["signature"] = kernel_name;

        std::filesystem::path json_path = env_trace_path;

        std::ofstream file(json_path);
        if (file) {
          file << json_.dump(4);
          LOG_DETAIL("Dumped kernel data to: {}", json_path.string());
        } else {
          LOG_DETAIL("Failed to write JSON to: {}", json_path.string());
        }

        LOG_DETAIL("Processed kernel: {}", kernel_name);
      }
    }

  } catch (const std::exception& e) {
    LOG_ERROR("Write object threw ", e.what());
  }
}

hsa_status_t nexus::hsa_amd_memory_pool_allocate(hsa_amd_memory_pool_t pool,
                                                 size_t size,
                                                 uint32_t flags,
                                                 void** ptr) {
  auto instance = get_instance();
  const auto result =
      hsa_ext_call(instance, hsa_amd_memory_pool_allocate, pool, size, flags, ptr);
  if (result == HSA_STATUS_SUCCESS && *ptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    instance->pointer_sizes_[*ptr] = size;
    LOG_DETAIL("HSA Allocated {} bytes at {}", size, static_cast<void*>(*ptr));
  }
  return result;
}
hsa_status_t nexus::hsa_memory_allocate(hsa_region_t region, size_t size, void** ptr) {
  auto instance = get_instance();
  const auto result = hsa_core_call(instance, hsa_memory_allocate, region, size, ptr);
  if (result == HSA_STATUS_SUCCESS && *ptr) {
    std::lock_guard<std::mutex> lock(mutex_);
    instance->pointer_sizes_[*ptr] = size;
    LOG_DETAIL("HSA Allocated {} bytes at {}", size, static_cast<void*>(*ptr));
  }
  return result;
}

hsa_status_t nexus::hsa_queue_create(hsa_agent_t agent,
                                     uint32_t size,
                                     hsa_queue_type32_t type,
                                     void (*callback)(hsa_status_t status,
                                                      hsa_queue_t* source,
                                                      void* data),
                                     void* data,
                                     uint32_t private_segment_size,
                                     uint32_t group_segment_size,
                                     hsa_queue_t** queue) {
  LOG_DETAIL("Creating nexus queue");

  hsa_status_t result = HSA_STATUS_SUCCESS;
  auto instance = get_instance();
  try {
    result = hsa_ext_call(instance,
                          hsa_amd_queue_intercept_create,
                          agent,
                          size,
                          type,
                          callback,
                          data,
                          private_segment_size,
                          group_segment_size,
                          queue);

    if (result == HSA_STATUS_SUCCESS) {
      auto result = instance->add_queue(*queue, agent);
      if (result != HSA_STATUS_SUCCESS) {
        LOG_ERROR("Failed to add queue {} ", static_cast<int>(result));
      }
      result = hsa_ext_call(instance,
                            hsa_amd_queue_intercept_register,
                            *queue,
                            nexus::on_submit_packet,
                            reinterpret_cast<void*>(*queue));
      if (result != HSA_STATUS_SUCCESS) {
        LOG_ERROR("Failed to register intercept callback with result of ",
                  static_cast<int>(result));
      }
    }
  } catch (const std::exception& e) {
    LOG_ERROR("Interception queue create throw {} error", e.what());
  }
  return result;
}
hsa_status_t nexus::hsa_queue_destroy(hsa_queue_t* queue) {
  LOG_DETAIL("Destroying nexus queue");
  return hsa_core_call(singleton_, hsa_queue_destroy, queue);
}
}  // namespace maestro

extern "C" {

PUBLIC_API bool OnLoad(HsaApiTable* table,
                       uint64_t runtime_version,
                       uint64_t failed_tool_count,
                       const char* const* failed_tool_names) {
  LOG_DETAIL("Creating maestro singleton");

  maestro::nexus* hook = maestro::nexus::get_instance(
      table, runtime_version, failed_tool_count, failed_tool_names);

  LOG_DETAIL("Creating maestro singleton completed");

  return true;
}

PUBLIC_API void OnUnload() {}

static void unload_me() __attribute__((destructor));
void unload_me() {}
}
