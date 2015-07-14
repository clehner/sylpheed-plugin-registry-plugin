# Sylpheed Plug-in Registry Plugin

This plug-in lets you access the [Sylpheed Plug-in
Registry](https://github.com/clehner/sylpheed-plugin-registry) and
install plug-ins from it in Sylpheed.

## Installation

Compile Sylpheed:

```
svn checkout svn://sylpheed.sraoss.jp/sylpheed/trunk sylpheed
./autogen.sh
./configure
make
```

Compile and install the plugin:

```
cd plugin
git clone https://github.com/clehner/sylpheed-plugin-registry-plugin
cd sylpheed-plugin-registry-plugin
make SYLPHEED_DIR=../../
make install
```

## Binaries

For binaries of this plug-in, check the
[releases](https://github.com/clehner/sylpheed-plugin-registry-plugin/releases)
page. On Linux x86\_64 with latest Sylpheed, you can copy the `registry.so`
file into your `~/.sylpheed-2.0/plugins/` directory and it should work.

## Usage

This plug-in augments Sylpheed's plug-in manager window, which is accessed from
the Configuration menu. It adds a registry panel with a list of plug-ins in the
registry. You can install with one click plug-ins that have binaries available
for your system, and uninstall plug-ins that you have installed. To refresh the
list of plugins in the window, click the "Check for update" button.
