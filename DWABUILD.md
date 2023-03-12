# DWA build notes
These notes are relevant only to internal DWA builds.

## Building
### Building against an exisiting/published moonray release
```bash
rez2
rez-env buildtools
rez-build -i -p <path_to_install_directory> --variants 0 --
```

### Building against a local/custom moonray build
You'll need to include the path of your custom moonray build in your **REZ_PACKAGES_PATH** env variable, and it is recommended to include the full version of your local moonray package that you wish to build against.
```bash
rez2
setenv REZ_LOCAL_PACKAGES_PATH <path_to_install_directory>
setenv REZ_PACKAGES_PATH ${REZ_LOCAL_PACKAGES_PATH}:${REZ_PACKAGES_PATH}
# include your full moonray package version in the rez-env cmd you use later for running rats
rez-env buildtools
# Note by setting REZ_LOCAL_PACKAGES_PATH you don't need the -p <path_to_install_directory> arg
rez-build -i --variants 0 --
```

