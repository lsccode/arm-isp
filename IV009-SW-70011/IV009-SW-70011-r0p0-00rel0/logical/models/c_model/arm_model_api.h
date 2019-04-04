//-----------------------------------------------------------------------------
// The confidential and proprietary information contained in this file may
// only be used by a person authorised under and to the extent permitted
// by a subsisting licensing agreement from Arm Limited or its affiliates.
//
// (C) COPYRIGHT 2005-2018 Arm Limited or its affiliates.
// ALL RIGHTS RESERVED
//
// This entire notice must be reproduced on all copies of this file
// and copies of this file may only be made by a person if such person is
// permitted to do so under the terms of a subsisting license agreement
// from Arm Limited or its affiliates.
//-----------------------------------------------------------------------------
//  Version and Release Control Information:
//
//  File Name           : arm_model_api.h
//  Release Package     : iv009_r0p0_00rel0
//-----------------------------------------------------------------------------
// Description:
//
//-----------------------------------------------------------------------------

#ifndef __ARM_MODEL_API_H__
#define __ARM_MODEL_API_H__
// ---------------------------------------------------------------------------------
#include <stdint.h>
#include <stddef.h>
/// @endcond
// ---------------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif
// ---------------------------------------------------------------------------------
typedef int32_t data_t;
// ---------------------------------------------------------------------------------
/// @cond
#ifdef _WIN32
    #ifdef ARM_MODEL_EXPORTS
        #define ARM_MODEL_API        __declspec(dllexport)
    #else
        #define ARM_MODEL_API        __declspec(dllimport)
    #endif
    #define LIBINIT
    #define LIBCLOSE
#else
    #define ARM_MODEL_API        __attribute__ ((visibility("default")))
    #define LIBINIT            __attribute__ ((constructor))
    #define LIBCLOSE        __attribute__ ((destructor))
#endif
/// @endcond
// ---------------------------------------------------------------------------------
/**
 *   Error values returned by following API functions
 */
typedef enum {
    ARM_MODEL_SUCCESS,                       /**< Success - no errors */
    ARM_MODEL_ERROR_NO_MEMORY,               /**< Not enough memory in the system */
    ARM_MODEL_ERROR_ADDRESS_NOT_ALIGNED,     /**<Address has to be aligned at 32-bits boundary*/
    ARM_MODEL_ERROR_CONFIG,                  /**<Configuration read/write error*/
    ARM_MODEL_ERROR_UNKNOWN                  /**<Unknown error, read stderr for more information*/
} arm_model_error_t;
// ---------------------------------------------------------------------------------
/**
 *   Structure defines image frames for processing
 */
typedef struct {

uint16_t width;      /**<Image width in pixels   */
uint16_t height;     /**<Image height in pixels   */
uint16_t planes;     /**<Number of planes (for example: 1 - RAW, 3 - RGB)   */
uint8_t depth;       /**<Actual number of bits per pixel per plane   */
size_t data_size;    /**<Size of data array in bytes   */
data_t* data;    /**<Array of pixels in layout: plane x X x Y, where X - width coordintate, Y - height coordinate
    for example 3x2 RGB:
    [R00 R01 R02]   [G00 G01 G02]   [B00 B01 B02]
    [R10 R11 R12]   [G10 G11 G12]   [B10 B11 B12]
    would be: [R00 G00 B00 R01 G01 B01 R02 G02 B02 R10 G10 B10 R11 G11 B11 R12 G12 B12]   */

} frame_t;
// ---------------------------------------------------------------------------------
typedef struct {
    uint16_t width;         /**<image width in pixels*/
    uint16_t height;        /**<image height in pixels*/
    uint16_t planes;        /**<number of planes (for example: 1 - RAW, 3 - RGB)*/
    uint8_t depth;          /**<actual number of bits per pixel per plane*/
    size_t data_size;       /**<size of data array in bytes*/
} frame_description_t;
// ---------------------------------------------------------------------------------
/**
 *   Structure defines requests for module's output
 */
#define MAX_MODULE_NAME 50
typedef struct {
    char module[MAX_MODULE_NAME];     /**< name of the module which output will be dumped.Max Length limited to MAX_MODULE_NAME*/
    frame_t frame;                    /**< dump output in this frame. Data must be prepared in advance*/
} dump_t;
// ---------------------------------------------------------------------------------

/**
 *      Set Trasaction Dump
 *
 *      This functions enables/disables trasaction dump feature.
 *
 *      @param[in]  trans_dump  :Bool True Enable trasaction Dump
 *      @return     error code
 */
ARM_MODEL_API arm_model_error_t arm_model_set_trans_dump(bool  trans_dump);
// ---------------------------------------------------------------------------------

/**
 *      Get Trasaction Dump
 *
 *      This functions gets the value of trasaction dump.
 *
 *      @param[out]  *trans_dump  :Bool True Enable trasaction Dump
 *      @return     error code
 */
ARM_MODEL_API arm_model_error_t arm_model_get_trans_dump(bool *trans_dump);
// ---------------------------------------------------------------------------------

/**
 *   Callback functions for external memory access
 *
 *   When set these callback functions provide access to memory for AXI modules in IP core
 *   Usually they read from and write to specific memory location imitating AXI bursts
 *
 *   @param buffer - pointer to memory with read/write data
 *   @param address - start address of external memory (usually AXI word aligned)
 *   @param size - data to be transfered in bytes (usually multiple of AXI word)
 *   @return nothing
 */
typedef void (*memory_read_cb)(uint8_t* buffer, uint32_t address, uint32_t size);
typedef void (*memory_write_cb)(const uint8_t* buffer, uint32_t address, uint32_t size);
// ---------------------------------------------------------------------------------

/**
 *   Set memory callback functions
 *
 *   The callback functions provided will be used for access to external memory by modules which use the AXI bus
 *   If the callbacks are set to NULL the IP core will use an internal memory representation
 *
 *   @param read_cb  - function for reading from external memory (might be NULL)
 *          write_cb - function for writing to external memory (might be NULL)
 *   @returns error code
 */
ARM_MODEL_API arm_model_error_t arm_model_set_memory_callbacks(memory_read_cb read_cb, memory_write_cb write_cb);
// ---------------------------------------------------------------------------------

/**
 *   Write data to DDR memory model
 *
 *   All transactions are 8-bits wide
 *   Sometimes output frames are written in DDR memory
 *
 *   @param data - pointer to buffer for data to be written
 *   @param address - start address
 *   @param items - number of bytes to write
 *   @returns error code
 */
ARM_MODEL_API arm_model_error_t arm_model_write_memory(const uint8_t* data, uint32_t address, uint32_t items);
// ---------------------------------------------------------------------------------

/**
 *   Read data from DDR memory model
 *
 *   The configuration item is named according to the configuration hierarchy
 *   See apical model script format for more details
 *
 *   @param data - pointer to buffer for data to be read
 *   @param address - start address
 *   @param items - number of bytes to read
 *   @returns error code
 */
ARM_MODEL_API arm_model_error_t arm_model_read_memory(uint8_t* data, uint32_t address, uint32_t items);
// ---------------------------------------------------------------------------------

/**
 *   Process script file
 *
 *   This function processes the images and configuration specified in a script file.
 *   See Apical script file format document for details
 *
 *   @param   infile - name of script file
 *   @param   outfile - name of file where read values are stored
 *   @param   debug_dump - if non-zero dump output data from all modules
 *   @param   trace_dump - show data processing path in stdout
 *   @returns error code
 */
ARM_MODEL_API arm_model_error_t arm_model_process_script(const char* infile, const char* outfile, int debug_dump, int trace_dump);
// ---------------------------------------------------------------------------------

/**
 *   Read data from configuration space
 *
 *   All transactions are 32-bits wide
 *   The registers and memory address map is described in documentation
 *
 *   @param data - pointer to buffer to save data
 *   @param address - start address aligned to 32-bits boundary
 *   @param items - number of 32-bits words to read
 *   @returns error code
 */
ARM_MODEL_API arm_model_error_t arm_model_read_config(uint32_t* data, uint32_t address, uint32_t items);
// ---------------------------------------------------------------------------------

/**
 *   Write data to configuration space
 *
 *   All transactions are 32-bits wide
 *   The registers and memory address map is described in documentation
 *
 *   @param data - pointer to buffer for data to be written
 *   @param address - start address aligned to 32-bits boundary
 *   @param items - number of 32-bits words to write
 *   @returns error code
 */
ARM_MODEL_API arm_model_error_t arm_model_write_config(const uint32_t* data, uint32_t address, uint32_t items);
// ---------------------------------------------------------------------------------

/**
 *   Read data from configuration space as strings
 *
 *   The configuration item is named according to the configuration hierarchy
 *   See apical model script format for more details
 *
 *   @param name - name of required items to read
 *   @param value - string to store the result
 *   @param value_length - length of value string
 *   @returns error code
 */
ARM_MODEL_API arm_model_error_t arm_model_read_config_string(const char* name, char* value, size_t value_length);
// ---------------------------------------------------------------------------------

/**
 *   Write data to configuration space as strings
 *
 *   The configuration item is named according to the configuration hierarchy
 *   See apical model script format for more details
 *
 *   @param name - name of required items to write
 *   @param value - expression to write (must be in hexadecimal format)
 *   @returns error code
 */
ARM_MODEL_API arm_model_error_t arm_model_write_config_string(const char* name, const char* value);
// ---------------------------------------------------------------------------------

/**
 *   Read value from configuration space image located in memory (not APB)
 *
 *   The configuration item is named according to the configuration hierarchy
 *   See apical model script format for more details
 *
 *   @param name - name of required item
 *   @param value - pointer to return value item
 *   @param offset - configuration image offset in the memory
 *   @returns error code
 */
ARM_MODEL_API arm_model_error_t arm_model_read_config_in_memory(const char* name, uint32_t* value, uint32_t offset);
// ---------------------------------------------------------------------------------

/**
 *   Write value to configuration space image located in memory (not APB)
 *
 *   The configuration item is named according to the configuration hierarchy
 *   See apical model script format for more details
 *
 *   @param name - name of required item
 *   @param value - value to write
 *   @param offset - configuration image offset in the memory
 *   @returns error code
 */
ARM_MODEL_API arm_model_error_t arm_model_write_config_in_memory(const char* name, uint32_t value, uint32_t offset);
// ---------------------------------------------------------------------------------

/**
 *   Process frames with current configuration
 *
 *   Number of input and output frames can be taken by arm_model_get_frames_number()
 *   If there is no output all frame dimensions will be zero
 *   Caller must preallocate memory for output frames and specify allocated memory size in corresponding field
 *
 *   @param frames_in - input frames for processing, number of frames can be taken by arm_model_get_frames_number()
 *   @param frames_out - preallocated storage for output frames, number of frames can be taken by arm_model_get_frames_number()
 *   @param dump - pointer to dump requests
 *   @param dump_num - number of requests in dump
 *   @param trace - if 1 print trace of frame coming through pipeline
 *   @returns error code
 */
ARM_MODEL_API arm_model_error_t arm_model_process_frames(const frame_t* frames_in, frame_t* frames_out, dump_t* dump, size_t dump_num, int trace);
ARM_MODEL_API arm_model_error_t arm_model_process_frames_desc(const frame_description_t* frames_desc_in, const data_t* frames_in, frame_description_t* frames_desc_out, data_t* frames_out);
// ---------------------------------------------------------------------------------

/**
 *   get number of input and output frames which has to be used in function arm_model_process_frames
 *
 *   Number of input and output frames are predefined
 *   If there is no output all frame dimensions will be zero
 *   Caller must preallocate memory for output frames and specify allocated memory size in corresponding field
 *
 *   @param  frames_in_num - pointer to variable for number of input frames
 *   @param  frames_out_num - pointer to variable for number of output frames
 *   @returns error code
 */
ARM_MODEL_API arm_model_error_t arm_model_get_frames_number(size_t* frames_in_num, size_t* frames_out_num);
// ---------------------------------------------------------------------------------

/**
 *   Reset IP core
 *
 *   Reset all modules in the IP core to their initial state and set configuration space to default values.
 *
 *   @param no parameters
 *   @returns error code
 */
ARM_MODEL_API arm_model_error_t arm_model_reset(void);
// ---------------------------------------------------------------------------------

/**
 *   Build info string
 *
 *   Returns pointer to the build info string
 *
 *   @param no parameters
 *   @returns constat pointer to C-style build info string
 */
ARM_MODEL_API const char * arm_model_build_info();
// ---------------------------------------------------------------------------------

/**
 *   Module info string
 *
 *   Returns pointer to the specified module info string
 *
 *   @param name - name of the module of interest (e.g. "iridix")
 *   @returns constat pointer to C-style module info string
 */
ARM_MODEL_API const char * arm_model_module_info(const char* name);
// ---------------------------------------------------------------------------------
#ifdef __cplusplus
}
#endif
// ---------------------------------------------------------------------------------
#endif /** __ARM_MODEL_API_H__ */
