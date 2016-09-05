/*
   Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012
   Her Majesty the Queen in Right of Canada (Communications Research
   Center Canada)

   Copyright (C), 2016, Matthias P. Braendli, matthias.braendli@mpb.li
 */
/*
   This file is part of ODR-DabMod.

   ODR-DabMod is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the
   License, or (at your option) any later version.

   ODR-DabMod is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with ODR-DabMod.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _LOG_H
#define _LOG_H

#ifdef HAVE_CONFIG_H
#   include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <syslog.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <list>
#include <stdexcept>
#include <string>
#include <map>
#include <mutex>
#include <thread>
#include <boost/lockfree/spsc_queue.hpp>

#define SYSLOG_IDENT "ODR-DabMod"
#define SYSLOG_FACILITY LOG_LOCAL0

enum log_level_t {debug = 0, info, warn, error, alert, emerg, trace};

static const std::string levels_as_str[] =
    { "     ", "     ", "WARN ", "ERROR", "ALERT", "EMERG", "TRACE"} ;

/** Abstract class all backends must inherit from */
class LogBackend {
    public:
        virtual void log(log_level_t level, std::string message) = 0;
        virtual std::string get_name() = 0;
};

/** A Logging backend for Syslog */
class LogToSyslog : public LogBackend {
    public:
        LogToSyslog() {
            name = "SYSLOG";
            openlog(SYSLOG_IDENT, LOG_PID, SYSLOG_FACILITY);
        }

        ~LogToSyslog() {
            closelog();
        }

        void log(log_level_t level, std::string message) {

            int syslog_level = LOG_EMERG;
            switch (level) {
                case trace: break; // Do not handle TRACE in syslog
                case debug: syslog_level = LOG_DEBUG; break;
                case info:  syslog_level = LOG_INFO; break;
                /* we don't have the notice level */
                case warn:  syslog_level = LOG_WARNING; break;
                case error: syslog_level = LOG_ERR; break;
                default:    syslog_level = LOG_CRIT; break;
                case alert: syslog_level = LOG_ALERT; break;
                case emerg: syslog_level = LOG_EMERG; break;
            }

            syslog(syslog_level, SYSLOG_IDENT " %s", message.c_str());
        }

        std::string get_name() { return name; }

    private:
        std::string name;

        LogToSyslog(const LogToSyslog& other) = delete;
        const LogToSyslog& operator=(const LogToSyslog& other) = delete;
};

class LogToFile : public LogBackend {
    public:
        LogToFile(std::string filename) {
            name = "FILE";

            log_file = fopen(filename.c_str(), "a");
            if (log_file == NULL) {
                fprintf(stderr, "Cannot open log file !");
                throw std::runtime_error("Cannot open log file !");
            }
        }

        ~LogToFile() {
            if (log_file != NULL) {
                fclose(log_file);
            }
        }

        void log(log_level_t level, std::string message);

        std::string get_name() { return name; }

    private:
        std::string name;
        FILE* log_file;

        LogToFile(const LogToFile& other) = delete;
        const LogToFile& operator=(const LogToFile& other) = delete;
};

class LogTracer : public LogBackend {
    public:
        LogTracer(const std::string& filename);

        ~LogTracer() {
            if (m_trace_file != NULL) {
                fclose(m_trace_file);
            }
        }

        void log(log_level_t level, std::string message);
        std::string get_name() { return name; }
    private:
        std::string name;
        uint64_t m_trace_micros_startup;
        FILE* m_trace_file;

        LogTracer(const LogTracer& other) = delete;
        const LogTracer& operator=(const LogTracer& other) = delete;
};

class LogLine;

struct log_message_t {
    log_message_t(log_level_t _level, std::string _message) :
        level(_level),
        message(_message) {}

    log_message_t() {}

    log_level_t level;
    std::string message;
};

class Logger {
    public:
        Logger() {
            m_io_thread = std::thread(&Logger::io_process, this);
        }

        Logger(const Logger& other) = delete;
        const Logger& operator=(const Logger& other) = delete;
        ~Logger() {
            // Special message to stop the thread
            log_message_t m(debug, "");

            m_message_queue.push(m);
            m_io_thread.join();
        }

        void register_backend(LogBackend* backend);

        /* Log the message to all backends */
        void log(log_level_t level, const char* fmt, ...);

        void logstr(log_level_t level, std::string message);

        /* All logging IO is done in another thread */
        void io_process(void);

        /* Return a LogLine for the given level
         * so that you can write etiLog.level(info) << "stuff = " << 21 */
        LogLine level(log_level_t level);

    private:
        std::list<LogBackend*> backends;

        boost::lockfree::spsc_queue<
            log_message_t,
            boost::lockfree::capacity<80> > m_message_queue;
        std::thread m_io_thread;
        std::mutex m_cerr_mutex;
};

extern Logger etiLog;

// Accumulate a line of logs, using same syntax as stringstream
// The line is logged when the LogLine gets destroyed
class LogLine {
    public:
        LogLine(const LogLine& logline);
        const LogLine& operator=(const LogLine& other) = delete;
        LogLine(Logger* logger, log_level_t level) :
            logger_(logger)
        {
            level_ = level;
        }

        // Push the new element into the stringstream
        template <typename T>
        LogLine& operator<<(T s) {
            os << s;
            return *this;
        }

        ~LogLine()
        {
            logger_->logstr(level_, os.str());
        }

    private:
        std::ostringstream os;
        log_level_t level_;
        Logger* logger_;
};


#endif

