#include "off/data/archive_vfs.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <utility>

namespace {
void check(bool value,const char* message) { if (!value) throw std::runtime_error(message); }
template<class F> void rejects(F f) {
  bool rejected=false; try { f(); } catch (const std::runtime_error&) { rejected=true; }
  check(rejected,"expected retained reader rejection");
}
void write(const std::filesystem::path& path,std::size_t count,char value) {
  std::ofstream file(path,std::ios::binary|std::ios::trunc);
  for (std::size_t i=0;i<count;++i) file.put(value);
  check(bool(file),"independent fixture write");
}
}
int main() {
  try {
    const std::filesystem::path work=OFF_VFS_READER_TEST_WORK_DIR;
    std::filesystem::create_directories(work);
    const auto path=work/"source.bin", moved_path=work/"retained-source.bin";
    std::filesystem::remove(moved_path);
    write(path,128,'a');
    off::data::ArchiveVfs vfs;
    static_cast<void>(vfs.mount_directory(work));
    const auto view=vfs.open_stream("source.bin");
    {
      auto reader=view.open_reader();
      std::array<std::byte,7> bytes{};
      reader.read_at(121,bytes);
      check(reader.size()==128 && bytes.front()==std::byte{'a'} && bytes.back()==std::byte{'a'},
            "retained reader validates same opened source extent");
      reader.read_at(128,{});
      rejects([&] { reader.read_at(129,{}); });
      rejects([&] { reader.read_at(122,bytes); });
      auto moved=std::move(reader);
      rejects([&] { reader.read_at(0,bytes); });
      moved.read_at(0,bytes);
      check(bytes[0]==std::byte{'a'},"move retains live file session");
      std::error_code error;
      std::filesystem::rename(path,moved_path,error);
      if (!error) {
        write(path,128,'b');
        moved.read_at(16,bytes);
        check(bytes[0]==std::byte{'a'},"path replacement does not retarget an opened reader");
        auto replacement=view.open_reader();
        replacement.read_at(16,bytes);
        check(bytes[0]==std::byte{'b'},"a newly opened reader uses current path");
      } else {
        // Some Windows file-sharing policies reject rename while a reader is
        // open. That also prevents replacement; do not claim it was exercised.
        std::cout<<"Open-file replacement test unavailable: "<<error.message()<<'\n';
      }
    }
    write(path,3,'c');
    rejects([&] { (void)view.open_reader(); });
    std::filesystem::remove(path);
    rejects([&] { (void)view.open_reader(); });
    std::filesystem::remove(moved_path);
    const auto short_path=work/"short-read.bin";
    write(short_path,256,'d');
    off::data::ArchiveVfs short_vfs;
    static_cast<void>(short_vfs.mount_directory(work));
    {
      auto short_reader=short_vfs.open_stream("short-read.bin").open_reader();
      std::error_code error;
      std::filesystem::resize_file(short_path,2,error);
      if (!error) {
        std::array<std::byte,8> bytes{};
        rejects([&] { short_reader.read_at(100,bytes); });
        rejects([&] { short_reader.read_at(0,{}); });
      } else {
        std::cout<<"Open-file truncation test unavailable: "<<error.message()<<'\n';
      }
    }
    std::filesystem::remove(short_path);
  } catch (const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
