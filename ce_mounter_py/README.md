## Tool for mounting archives

The currently used tool is specified in the **.env** file:
> MOUNT_ARCHIVES_TOOL=fuse-archive

We were using **fuse-zip** before, but that supported only **ZIP** files, so it
was replaces with **fuse-archive** which supports more formats.

Mounter uses **fuse-archive tool** for mounting all kinds of archives. At the time of 
adding this to mounter this couldn't be installed using **apt-get install**, so do 
the following to install it from sources:
 
```
sudo apt install libarchive-dev libfuse-dev
git clone https://github.com/google/fuse-archive.git
cd fuse-archive
make
sudo make install
```

If you get 
> "undefined reference to `__atomic_load_8'" during compilation

...add **-latomic** to Makefile near / in $(LDFLAGS).

If you ever change this archive tool to another one, or the supported archives in this
tool change, don't forget to update the list of supported archive extensions,
which we can mount, in the **.env** file:
> MOUNT_ARCHIVES_SUPPORTED=.7z,.cab,.iso,.zip,.rar,.tar,.tar.bz2,.tar.gz,.zip,.bz2,.gz

Please note that the dot ('.') in front of the extensions is intentional - it helps to
match that extension at the end of filename and not somewhere in the middle.
