/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010-2013 Facebook, Inc. (http://www.facebook.com)     |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#include "hphp/util/embedded-data.h"
#include "hphp/util/current-executable.h"

#include "folly/ScopeGuard.h"

#include <libelf.h>
#include <gelf.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/getsect.h>
#endif

namespace HPHP { namespace Util {

bool get_embedded_data(const char *section, embedded_data* desc) {
#ifndef __APPLE__
  GElf_Shdr shdr;
  size_t shstrndx;
  char *name;
  Elf_Scn *scn;

  if (elf_version(EV_CURRENT) == EV_NONE) return false;

  int fd = open(current_executable_path().c_str(), O_RDONLY, 0);
  if (fd < 0) return false;
  SCOPE_EXIT { close(fd); };

  Elf* e = elf_begin(fd, ELF_C_READ, nullptr);

  if (!e ||
      elf_kind(e) != ELF_K_ELF ||
#ifdef HAVE_ELF_GETSHDRSTRNDX
      elf_getshdrstrndx(e, &shstrndx)
#else
      !elf_getshstrndx(e, &shstrndx)
#endif
      ) {
    return false;
  }
  scn = nullptr;
  while ((scn = elf_nextscn(e, scn)) != nullptr) {
    if (gelf_getshdr(scn, &shdr) != &shdr ||
        !(name = elf_strptr(e, shstrndx , shdr.sh_name))) {
      return false;
    }
    if (!strcmp(section, name)) {
      GElf_Shdr ghdr;
      if (gelf_getshdr(scn, &ghdr) != &ghdr) return false;
      desc->m_filename = current_executable_path();
      desc->m_start = ghdr.sh_offset;
      desc->m_len = ghdr.sh_size;
      return true;
    }
  }
#else // __APPLE__
  const struct section_64 *sect = getsectbyname("__text", section);
  if (sect) {
    std::string path = current_executable_path();
    if (!path.empty()) {
      desc->m_filename = path;
    } else {
      return false;
    }
    desc->m_start = sect->offset;
    desc->m_len = sect->size;
    return true;
  }
#endif // __APPLE__
  return false;
}

} }