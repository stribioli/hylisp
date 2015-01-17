## Installing

The three layers must be installed separately and sequentially (*bottom-up*).

First of all you have to download *hyLISP*. You can either checkout the Git repository using

`git clone https://github.com/sassospicco/hylisp.git`

or get and unpack the package using

`fetch http://hylisp.org/download/hylisp-<version>.tar.gz`

`tar -vxzf hylisp-<version>.tar.gz`

### Data plane

* Login as **root**.
* Make sure your FreeBSD release is a supported one (you can check with `freebsd-version`). Currently, only `10.1-RELEASE` is supported.
* Make sure you have the source tree installed on your machine under `/usr/src`. If not, you can
	* Download it from FreeBSD repository using `fetch ftp://ftp.freebsd.org/pub/FreeBSD/releases/<arch>/<release>/src.txz`
	* Extract it using `tar -C / -vxJf src.txz`
* Move to the `hylisp-dp` directory and launch the install script with `./install.sh`. This will patch the kernel source and copy the new header files to `/usr/include`. It will also add a simple kernel configuration named *LISP* inheriting from *GENERIC*. If you already have a tuned kernel, edit it as you please.
* Move to `/usr/src` and rebuild+install the patched kernel with `make kernel KERNCONF=LISP`.
* Note that routing functions are disabled by default in FreeBSD. You have to enable them by adding a `gateway_enable="YES"` line (and `ipv6_gateway_enable="YES"` as well, if necessary) in `/etc/rc.conf`.
* Reboot and you should be done.

* **Optional**: the original OpenLISP tools (`map`, `mapstat` and various `man` pages) are available, but no guarantee is made about their stability. You can patch the source tree running the script `./install-tools.sh` and build+install with

		cd /usr/src/sbin/map/
		make depend
		make
		make install
		cd /usr/src/usr.bin/mapstat/
		make depend
		make
		make install
		cd /usr/src/share/man/man4/
		make
		make install

### Hypervisor

* Move to the `hylisp-hv` directory and issue `make` and `make install`
* A `make install-service` will install a `rc.d` service. Add `hylisphv_enable="YES"` to `/etc/rc.conf` to enable it.

### Control plane

* Make sure you have *expat* library installed (check if `/usr/local/include/expat.h` exists). Otherwise, install it with `pkg install expat` or `cd /usr/ports/textproc/expat && make clean install`.
* Move to the `hylisp-cp` directory and issue `make` and `make install`