#ifndef __CONF_H__
#define __CONF_H__

#include<stdio.h>
#include<limits.h>
#include<netinet/in.h>

/* ============================== Config Options ==============================
 * Syntax:
 * The config file is a list of 'name=value' pairs separated by lines.
 * Only one option per line, spaces before, after and between '=' separator
 * are ignored. # defines the start of a comment.
 * 
 * Options:
 * 	IP = [ipv4 address] # The ip address the server will listen on.
 * Default is 0.0.0.0
 * 	Port = [port] # The port the server will listen on.
 * Default is 80 (or 8080 for debug)
 * 	Index = [filename] # A name of the index file to use if a resolved
 * URL path is a folder. Default is 'index.html'.
 * 	BaseDir = [path] # A path(can be relative to the working directory) to
 * the directory from which to server the website files.
 * Default is '.' (Current Working Directory).
 * ============================================================================
*/

// ==================== Constants/Defines ====================

/**Path to system-wide configuration file.*/
#define CONFIG_PATH_SYS "/etc/arachno.conf"
/**Relative path to user configuration file.*/
#define CONFIG_PATH_USR "./arachno.conf"
//#define CONFIG_DIR_SYS "/etc/arachno.d"

/**Maximum line size for the config file.*/
#define CONFIG_MAX_LINE_SIZE 1024

// ==================== Configuration Variables ====================

/**Local ip of the server to listen on. Default = 0.0.0.0*/
extern struct in_addr cfg_local_ip;
/**Local port of the server to listen on. Default = 8080/80 (Debug/Release)*/
extern in_port_t cfg_local_port;
/**The default index file for when url path is a dir. Default = index.html*/
extern char cfg_default_index[FILENAME_MAX];
/**Length of cfg_default_index.*/
extern size_t cfg_default_index_len;
/**The base dir of the website. Default = '.' (Current Working Directory)*/
extern char cfg_base_path[PATH_MAX];
/**Length of cfg_base_path.*/
extern size_t cfg_base_path_len;

// ==================== Functions ====================

/** Find and load all appropriate config files from CONFIG_PATH_SYS,
 * CONFIG_DIR(if defined) and CONFIG_PATH_USR. In that order.
 * Prints errors to stderr on failure.
 * @return 0 on success, -1 on failure.
*/
int loadConfigs();

/** Load a specified config file.
 * @param path The path to the config file.
 * @return 0 on success, -1 on failure.
*/
int loadConfig(const char *path);

#endif