# D.A.R.W.I.N

Darwin is an open source **Artificial Intelligence Framework for CyberSecurity**. It can be compiled and run on **both FreeBSD and Linux**.

We provide packages and support for FreeBSD.

Darwin is:
 - A multi-threaded C++ engine that runs **security filters** that work together to improve your network security
 - A collection of **agents** that use the DARWIN protocol to query the security filters accordingly

Darwin is still in an alpha stage, so few filters are available at this time.

Using the provided documentation and SDK you can develop your own Darwin Filters.
We are seeking help! Testers and volunteers are welcome!

Advens (www.advens.fr) also provides commercial filters for Darwin !

## Filters

### Compilation

*Note: This code part follows the C++14 standard. Compile with g++ version 8.3.0 or later.*

To compile all the filters available, please enter the following:

```sh
cmake .
make -j4
```

To compile a specific filter:

```sh
cmake . -DFILTER=FILTER_NAME
make -j4
```

You can choose a filter from this [list](https://github.com/VultureProject/darwin/wiki/Filters-Summary)

You can also set a filter list:

```sh
cmake . -DFILTER="FILTER_NAME1;FILTER_NAME2"
make -j4
```

Don't forget to unset the `FILTER` variable if you want to compile all the filters available afterwards:

```sh
cmake . -UFILTER
make -j4
```

The compiled filter will be named `darwin_filter_name` (*note: the name is displayed at the beginning of the compilation*).
You will find compilation and dependencies informations for each filters in the [Wiki](https://github.com/VultureProject/darwin/wiki).

### Usage

Usage: `./darwin filter_name socket_path config_file monitoring_socket_path pid_file output next_filter_socket_path nb_thread cache_size threshold[OPTION]`

Positional arguments:
- `filter_name` Specify the name of this filter in the logs
- `socket_path` Specify the path to the unix socket for the main connection
- `config_file` Specify the path to the configuration file
- `monitoring_socket_path` Specify the path to the monitoring unix socket
- `pid_file` Specify the path to the file containing the pid of the process
- `output` Specify the filter's output
- `next_filter_socket_path` Specify the path to the next filter unix socket
- `nb_thread` Integer specifying the number of treatment thread for this process
- `cache_size` Integer specifying cache's size
- `threshold` Integer specifying the filter's threshold (if behind 100, take the filter's default threshold)

OPTIONS:
- `-d` Debug mode, does not create a daemon, set log level to debug
- `-i` Set log level to info
- `-w` Set log level to warning (default)
- `-e` Set log level to error
- `-c` Set log level to critical

*Note: The only option taken is the one set after `threshold`.*

*Note: All filter currently log in the same file `/var/log/darwin/darwin.log`*

*Note: All filter write a pid file in the following path: `/var/run/darwin/filter_name.pid`*

## Filter Manager

### Python Version

Compatible with python 3.5.3 and later.

### Usage

Usage: `manager.py [-h] [-l {DEBUG,INFO,WARNING,ERROR,CRITICAL}] config_file`

Positional arguments:

`config_file` The config file to use.

Optional arguments:

`-h`, `--help` show this help message and exit

`-l {DEBUG,INFO,WARNING,ERROR,CRITICAL}`, `--log-level {DEBUG,INFO,WARNING,ERROR,CRITICAL}`
Set log level to DEBUG, INFO, WARNING (default), ERROR or CRITICAL.

### Config File

The config file is JSON formatted and contains the filters information. They *MUST* be formatted as follow:

```json
{
  "session_1": {
    "exec_path": "/home/darwin/filters/darwin_session",
    "config_file": "/var/sockets/redis/redis.sock",
    "output": "LOG",
    "next_filter": "",
    "nb_thread": 5,
    "threshold": 80,
    "log_level": "DEBUG",
    "cache_size": 0
  },
  "dga_1": {
    "exec_path": "/home/darwin/filters/darwin_dga",
    "config_file": "/home/darwin/conf/fdga/fdga.conf",
    "output": "LOG",
    "next_filter": "",
    "nb_thread": 5,
    "log_level": "DEBUG",
    "cache_size": 0
  }
}
```
You will find more information in the [Wiki](https://github.com/VultureProject/darwin/wiki/Darwin-configuration) 

## The Service

In the service directory is an rc script named `darwin` that is
the service script. It handles the following commands: `start`, `stop`,
`status` and `restart`.


## Usage

*Use this for debug purpose only.*

Usage: `manager.py [-h] [-l {DEBUG,INFO,WARNING,ERROR,CRITICAL}] config_file`

Positional arguments:

`config_file` The config file to use.

Optional arguments:

`-h`, `--help` show this help message and exit

`-l {DEBUG,INFO,WARNING,ERROR,CRITICAL}`, `--log-level {DEBUG,INFO,WARNING,ERROR,CRITICAL}`
Set log level to DEBUG, INFO, WARNING (default), ERROR or CRITICAL.
