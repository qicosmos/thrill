/*******************************************************************************
 * thrill/io/config_file.cpp
 *
 * Copied and modified from STXXL https://github.com/stxxl/stxxl, which is
 * distributed under the Boost Software License, Version 1.0.
 *
 * Part of Project Thrill - http://project-thrill.org
 *
 * Copyright (C) 2002 Roman Dementiev <dementiev@mpi-sb.mpg.de>
 * Copyright (C) 2007, 2009 Johannes Singler <singler@ira.uka.de>
 * Copyright (C) 2008, 2009 Andreas Beckmann <beckmann@cs.uni-frankfurt.de>
 * Copyright (C) 2013 Timo Bingmann <tb@panthema.net>
 *
 * All rights reserved. Published under the BSD-2 license in the LICENSE file.
 ******************************************************************************/

#include <thrill/common/cmdline_parser.hpp>
#include <thrill/common/config.hpp>
#include <thrill/io/config_file.hpp>
#include <thrill/io/error_handling.hpp>
#include <thrill/io/file_base.hpp>

#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#if THRILL_WINDOWS
   #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#else
  #include <unistd.h>
#endif

namespace thrill {
namespace io {

static inline bool exist_file(const std::string& path) {
    LOG0 << "Checking " << path << " for disk configuration.";
    std::ifstream in(path.c_str());
    return in.good();
}

Config::~Config() {
    for (disk_list_type::const_iterator it = disks_list.begin();
         it != disks_list.end(); it++)
    {
        if (it->delete_on_exit)
        {
            std::cerr << "Thrill: removing disk file: " << it->path << std::endl;
            unlink(it->path.c_str());
        }
    }
}

void Config::initialize() {
    // if disks_list is empty, then try to load disk configuration files
    if (disks_list.size() == 0)
    {
        find_config();
    }

    max_device_id_ = 0;

    is_initialized = true;
}

void Config::find_config() {
    // check several locations for disk configuration files

    // check THRILL_CONFIG environment path
    const char* thrill_cfg = getenv("THRILL_CONFIG");
    if (thrill_cfg && exist_file(thrill_cfg))
        return load_config_file(thrill_cfg);

#if !THRILL_WINDOWS
    // read environment, unix style
    const char* hostname = getenv("HOSTNAME");
    const char* home = getenv("HOME");
    const char* suffix = "";
#else
    // read environment, windows style
    const char* hostname = getenv("COMPUTERNAME");
    const char* home = getenv("APPDATA");
    const char* suffix = ".txt";
#endif

    // check current directory
    {
        std::string basepath = "./.thrill";

        if (hostname && exist_file(basepath + "." + hostname + suffix))
            return load_config_file(basepath + "." + hostname + suffix);

        if (exist_file(basepath + suffix))
            return load_config_file(basepath + suffix);
    }

    // check home directory
    if (home)
    {
        std::string basepath = std::string(home) + "/.thrill";

        if (hostname && exist_file(basepath + "." + hostname + suffix))
            return load_config_file(basepath + "." + hostname + suffix);

        if (exist_file(basepath + suffix))
            return load_config_file(basepath + suffix);
    }

    // load default configuration
    load_default_config();
}

void Config::load_default_config() {
    std::cerr << "Thrill: no config file ~/.thrill found, "
              << "using default disk configuration." << std::endl;
#if !THRILL_WINDOWS
    DiskConfig entry1("/tmp/thrill.tmp", 1000 * 1024 * 1024, "syscall");
    entry1.unlink_on_open = true;
    entry1.autogrow = true;
#else
    DiskConfig entry1("", 1000 * 1024 * 1024, "wincall");
    entry1.delete_on_exit = true;
    entry1.autogrow = true;

    char* tmpstr = new char[255];
    if (GetTempPath(255, tmpstr) == 0)
        THRILL_THROW_WIN_LASTERROR(IoError, "GetTempPath()");
    entry1.path = tmpstr;
    entry1.path += "thrill.tmp";
    delete[] tmpstr;
#endif
    disks_list.push_back(entry1);

    // no flash disks
    first_flash = (unsigned int)disks_list.size();
}

void Config::load_config_file(const std::string& config_path) {
    std::vector<DiskConfig> flash_list;
    std::ifstream cfg_file(config_path.c_str());

    if (!cfg_file)
        return load_default_config();

    std::string line;

    while (std::getline(cfg_file, line))
    {
        // skip comments
        if (line.size() == 0 || line[0] == '#') continue;

        DiskConfig entry;
        entry.parse_line(line); // throws on errors

        if (!entry.flash)
            disks_list.push_back(entry);
        else
            flash_list.push_back(entry);
    }
    cfg_file.close();

    // put flash devices after regular disks
    first_flash = (unsigned int)disks_list.size();
    disks_list.insert(disks_list.end(), flash_list.begin(), flash_list.end());

    if (disks_list.empty()) {
        THRILL_THROW(std::runtime_error,
                     "No disks found in '" << config_path << "'.");
    }
}

//! Returns automatic physical device id counter
unsigned int Config::get_max_device_id() {
    return max_device_id_;
}

//! Returns next automatic physical device id counter
unsigned int Config::get_next_device_id() {
    return max_device_id_++;
}

//! Update the automatic physical device id counter
void Config::update_max_device_id(unsigned int devid) {
    if (max_device_id_ < devid + 1)
        max_device_id_ = devid + 1;
}

uint64_t Config::total_size() const {
    assert(is_initialized);

    uint64_t total_size = 0;

    for (disk_list_type::const_iterator it = disks_list.begin();
         it != disks_list.end(); it++)
    {
        total_size += it->size;
    }

    return total_size;
}

////////////////////////////////////////////////////////////////////////////////

DiskConfig::DiskConfig()
    : size(0),
      autogrow(true),
      delete_on_exit(false),
      direct(DIRECT_TRY),
      flash(false),
      queue(FileBase::DEFAULT_QUEUE),
      device_id(FileBase::DEFAULT_DEVICE_ID),
      raw_device(false),
      unlink_on_open(false),
      queue_length(0)
{ }

DiskConfig::DiskConfig(const std::string& _path, uint64_t _size,
                       const std::string& _io_impl)
    : path(_path),
      size(_size),
      io_impl(_io_impl),
      autogrow(true),
      delete_on_exit(false),
      direct(DIRECT_TRY),
      flash(false),
      queue(FileBase::DEFAULT_QUEUE),
      device_id(FileBase::DEFAULT_DEVICE_ID),
      raw_device(false),
      unlink_on_open(false),
      queue_length(0) {
    parse_fileio();
}

DiskConfig::DiskConfig(const std::string& line)
    : size(0),
      autogrow(true),
      delete_on_exit(false),
      direct(DIRECT_TRY),
      flash(false),
      queue(FileBase::DEFAULT_QUEUE),
      device_id(FileBase::DEFAULT_DEVICE_ID),
      raw_device(false),
      unlink_on_open(false),
      queue_length(0) {
    parse_line(line);
}

void DiskConfig::parse_line(const std::string& line) {
    // split off disk= or flash=
    std::vector<std::string> eqfield = common::Split(line, "=", 2, 2);

    if (eqfield[0] == "disk") {
        flash = false;
    }
    else if (eqfield[0] == "flash") {
        flash = true;
    }
    else {
        THRILL_THROW(std::runtime_error,
                     "Unknown configuration token " << eqfield[0]);
    }

    // *** Set Default Extra Options ***

    autogrow = true; // was default for a long time, have to keep it this way
    delete_on_exit = false;
    direct = DIRECT_TRY;
    // flash is already set
    queue = FileBase::DEFAULT_QUEUE;
    device_id = FileBase::DEFAULT_DEVICE_ID;
    unlink_on_open = false;

    // *** Save Basic Options ***

    // split at commands, at least 3 fields
    std::vector<std::string> cmfield = common::Split(eqfield[1], ",", 3, 3);

    // path:
    path = cmfield[0];
    // replace $$ -> pid in path
    {
        std::string::size_type pos;
        if ((pos = path.find("$$")) != std::string::npos)
        {
#if !THRILL_WINDOWS
            int pid = getpid();
#else
            DWORD pid = GetCurrentProcessId();
#endif
            path.replace(pos, 2, std::to_string(pid));
        }
    }

    // size: (default unit MiB)
    if (!common::ParseSiIecUnits(cmfield[1].c_str(), size, 'M')) {
        THRILL_THROW(std::runtime_error,
                     "Invalid disk size '" << cmfield[1] << "' in disk configuration file.");
    }

    if (size == 0) {
        autogrow = true;
        delete_on_exit = true;
    }

    // io_impl:
    io_impl = cmfield[2];
    parse_fileio();
}

void DiskConfig::parse_fileio() {
    // skip over leading spaces
    size_t leadspace = io_impl.find_first_not_of(' ');
    if (leadspace > 0)
        io_impl = io_impl.substr(leadspace);

    // split off extra fileio parameters
    size_t spacepos = io_impl.find(' ');
    if (spacepos == std::string::npos)
        return; // no space in fileio

    // *** Parse Extra Fileio Parameters ***

    std::string paramstr = io_impl.substr(spacepos + 1);
    io_impl = io_impl.substr(0, spacepos);

    std::vector<std::string> param = common::Split(paramstr, " ");

    for (std::vector<std::string>::const_iterator p = param.begin();
         p != param.end(); ++p)
    {
        // split at equal sign
        std::vector<std::string> eq = common::Split(*p, "=", 2, 2);

        // *** PLEASE try to keep the elseifs sorted by parameter name!
        if (*p == "") {
            // skip blank options
        }
        else if (*p == "autogrow" || *p == "noautogrow" || eq[0] == "autogrow")
        {
            // TODO(?): which fileio implementation support autogrow?

            if (*p == "autogrow") autogrow = true;
            else if (*p == "noautogrow") autogrow = false;
            else if (eq[1] == "off") autogrow = false;
            else if (eq[1] == "on") autogrow = true;
            else if (eq[1] == "no") autogrow = false;
            else if (eq[1] == "yes") autogrow = true;
            else
            {
                THRILL_THROW(std::runtime_error,
                             "Invalid parameter '" << *p << "' in disk configuration file.");
            }
        }
        else if (*p == "delete" || *p == "delete_on_exit")
        {
            delete_on_exit = true;
        }
        else if (*p == "direct" || *p == "nodirect" || eq[0] == "direct")
        {
            // io_impl is not checked here, but I guess that is okay for DIRECT
            // since it depends highly platform _and_ build-time configuration.

            if (*p == "direct") direct = DIRECT_ON;          // force ON
            else if (*p == "nodirect") direct = DIRECT_OFF;  // force OFF
            else if (eq[1] == "off") direct = DIRECT_OFF;
            else if (eq[1] == "try") direct = DIRECT_TRY;
            else if (eq[1] == "on") direct = DIRECT_ON;
            else if (eq[1] == "no") direct = DIRECT_OFF;
            else if (eq[1] == "yes") direct = DIRECT_ON;
            else
            {
                THRILL_THROW(std::runtime_error,
                             "Invalid parameter '" << *p << "' in disk configuration file.");
            }
        }
        else if (eq[0] == "queue")
        {
            if (io_impl == "linuxaio") {
                THRILL_THROW(std::runtime_error, "Parameter '" << *p << "' invalid for fileio '" << io_impl << "' in disk configuration file.");
            }

            char* endp;
            queue = static_cast<int>(strtoul(eq[1].c_str(), &endp, 10));
            if (endp && *endp != 0) {
                THRILL_THROW(std::runtime_error,
                             "Invalid parameter '" << *p << "' in disk configuration file.");
            }
        }
        else if (eq[0] == "queue_length")
        {
            if (io_impl != "linuxaio") {
                THRILL_THROW(std::runtime_error, "Parameter '" << *p << "' "
                             "is only valid for fileio linuxaio "
                             "in disk configuration file.");
            }

            char* endp;
            queue_length = static_cast<int>(strtoul(eq[1].c_str(), &endp, 10));
            if (endp && *endp != 0) {
                THRILL_THROW(std::runtime_error,
                             "Invalid parameter '" << *p << "' in disk configuration file.");
            }
        }
        else if (eq[0] == "device_id" || eq[0] == "devid")
        {
            char* endp;
            device_id = static_cast<int>(strtoul(eq[1].c_str(), &endp, 10));
            if (endp && *endp != 0) {
                THRILL_THROW(std::runtime_error,
                             "Invalid parameter '" << *p << "' in disk configuration file.");
            }
        }
        else if (*p == "raw_device")
        {
            if (!(io_impl == "syscall")) {
                THRILL_THROW(std::runtime_error, "Parameter '" << *p << "' invalid for fileio '" << io_impl << "' in disk configuration file.");
            }

            raw_device = true;
        }
        else if (*p == "unlink" || *p == "unlink_on_open")
        {
            if (!(io_impl == "syscall" || io_impl == "linuxaio" ||
                  io_impl == "mmap" || io_impl == "wbtl"))
            {
                THRILL_THROW(std::runtime_error, "Parameter '" << *p << "' invalid for fileio '" << io_impl << "' in disk configuration file.");
            }

            unlink_on_open = true;
        }
        else
        {
            THRILL_THROW(std::runtime_error,
                         "Invalid optional parameter '" << *p << "' in disk configuration file.");
        }
    }
}

std::string DiskConfig::fileio_string() const {
    std::ostringstream oss;

    oss << io_impl;

    if (!autogrow)
        oss << " autogrow=no";

    if (delete_on_exit)
        oss << " delete_on_exit";

    // tristate direct variable: OFF, TRY, ON
    if (direct == DIRECT_OFF)
        oss << " direct=off";
    else if (direct == DIRECT_TRY) {
        // silenced: oss << " direct=try";
    }
    else if (direct == DIRECT_ON)
        oss << " direct=on";
    else
        THRILL_THROW(std::runtime_error, "Invalid setting for 'direct' option.");

    if (flash)
        oss << " flash";

    if (queue != FileBase::DEFAULT_QUEUE && queue != FileBase::DEFAULT_LINUXAIO_QUEUE)
        oss << " queue=" << queue;

    if (device_id != FileBase::DEFAULT_DEVICE_ID)
        oss << " devid=" << device_id;

    if (raw_device)
        oss << " raw_device";

    if (unlink_on_open)
        oss << " unlink_on_open";

    if (queue_length != 0)
        oss << " queue_length=" << queue_length;

    return oss.str();
}

} // namespace io
} // namespace thrill

/******************************************************************************/
