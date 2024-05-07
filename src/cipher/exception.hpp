#pragma once

#include "common/platform.hpp"

#include <stdexcept>
#include <sstream>
#include <iomanip>


namespace fb2k_ncm::cipher
{
    class cipher_error : public std::runtime_error {
    public:
        explicit cipher_error(std::string reason, STATUS status) : std::runtime_error(""), status_(status), reason_(std::move(reason)) {
            last_error_ = GetLastError();
            std::stringstream err_ss;
            msg_ = fmtlib::format("cipher runtime error because of [{}] \n  Last Error: 0x{:08X}\n  STATUS: 0x{:08X}",
                               reason_,
                               last_error_,
                               static_cast<uint32_t>(status_));
        }
        const char *what() const noexcept override { return msg_.c_str(); }

    public:
        STATUS status_;
        DWORD last_error_;

    protected:
        std::string reason_;
        std::string msg_;
    };
} // namespace fb2k_ncm::cipher
