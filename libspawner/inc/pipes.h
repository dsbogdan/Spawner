#ifndef _SPAWNER_PIPES_H_
#define _SPAWNER_PIPES_H_

#include "platform.h"
#include <sstream>
#include <fstream>
#include <string>

enum pipes_t//rename
{
    STD_INPUT_PIPE,
    STD_OUTPUT_PIPE,
    STD_ERROR_PIPE
};

enum std_pipe_t
{
    PIPE_INPUT,
    PIPE_OUTPUT,

    PIPE_UNDEFINED,
};

class input_buffer_class {
protected:
    size_t buffer_size;
public:
    input_buffer_class();
    input_buffer_class(const size_t &buffer_size_param);
    virtual bool readable() {
        return false;
    }
    virtual size_t read(void *data, size_t size) {
        return 0;
    }
};
class output_buffer_class {
protected:
    size_t buffer_size;
public:
    output_buffer_class();
    output_buffer_class(const size_t &buffer_size_param);
    virtual bool writeable() {
        return false;
    }
    virtual size_t write(void *data, size_t size) {
        return 0;
    }
};

class handle_buffer_class {
protected:
    handle_t stream;
    size_t protected_read(void *data, size_t size);
    size_t protected_write(void *data, size_t size);
    void init_handle(handle_t stream_arg);
public:
    handle_buffer_class();
    ~handle_buffer_class();
};

class input_file_buffer_class: public input_buffer_class, protected handle_buffer_class {
public:
    input_file_buffer_class();
    input_file_buffer_class(const std::string &file_name, const size_t &buffer_size_param);
    virtual bool readable();
    virtual size_t read(void *data, size_t size);
};

class output_file_buffer_class: public output_buffer_class, protected handle_buffer_class {
public:
    output_file_buffer_class();
    output_file_buffer_class(const std::string &file_name, const size_t &buffer_size_param);
    virtual bool writeable();
    virtual size_t write(void *data, size_t size);
};

class output_stdout_buffer_class: public output_buffer_class, protected handle_buffer_class {
public:
    output_stdout_buffer_class();
    output_stdout_buffer_class(const size_t &buffer_size_param);
    virtual bool writeable();
    virtual size_t write(void *data, size_t size);
};

class pipe_class
{
protected:
    bool create_pipe();
    handle_t buffer_thread;
    handle_t reading_mutex;
    pipe_t readPipe, writePipe;
    std_pipe_t pipe_type;
	std::string file_name;
    bool state;
    pipe_t input_pipe(){ return readPipe;}
    pipe_t output_pipe(){ return writePipe;}
public:
    pipe_class();
    pipe_class(std_pipe_t pipe_type);
	pipe_class(std::string file_name, std_pipe_t pipe_type);
    void close_pipe();
    ~pipe_class();
    size_t write(void *data, size_t size);
    size_t read(void *data, size_t size);
    virtual bool bufferize();
    void wait();
    void finish();
    /* think about safer way of reading from pipe */
    void wait_for_pipe(const unsigned int &ms_time);
    void safe_release();
    bool valid();
    virtual pipe_t get_pipe();
};

class input_pipe_class: public pipe_class
{
protected:
    input_buffer_class *input_buffer;
    static thread_return_t writing_buffer(thread_param_t param);
public:
    input_pipe_class();
    input_pipe_class(input_buffer_class *input_buffer_arg);
    virtual bool bufferize();
    virtual pipe_t get_pipe();
};

class output_pipe_class: public pipe_class
{
protected:
    output_buffer_class *output_buffer;
    static thread_return_t reading_buffer(thread_param_t param);
public:
    output_pipe_class();
    output_pipe_class(output_buffer_class *output_buffer_arg);
    virtual bool bufferize();
    virtual pipe_t get_pipe();
};

#endif//_SPAWNER_PIPES_H_