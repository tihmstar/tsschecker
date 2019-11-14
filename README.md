# tsschecker  
_tsschecker is a powerful tool to check TSS signing status of various iOS/iPadOS/tvOS/watchOS devices and versions._

the Latest compiled version can be found here: https://github.com/tihmstar/tsschecker/releases

Tihmstar's latest build can be found here:    
(MacOS and Linux)    
https://api.tihmstar.net/builds/tsschecker/tsschecker-latest.zip

Follow [this guide](https://dev.to/jake/using-libcurl3-and-libcurl4-on-ubuntu-1804-bionic-184g) to use tsschecker on Ubuntu 18.04 (Bionic) as it requires libcurl3 which cannot coexist with libcurl4 on this OS.

## Features  
* Allows you to get lists of all devices and all iOS/OTA versions for a specific device.
* Can check signing status for default iOS versions and beta ipsws (by specifying a `BuildManifest.plist`)
* Works without specifying any device relevant values to check signing status, but can be used to save blobs when given an ECID and the option `--print-tss-response` (although there are better tools to do this).

tsschecker not only can be used to check firmware signing status or save APTickets, but can also explore Apple's TSS servers.
By using all of its customization possibilities, you might discover a combination of devices and iOS versions that is now getting signed but wasn't getting signed before. 

### About APNonces:
_default generators for saving APTickets:_
* `0xbd34a880be0b53f3` - used on the Electra and Chimera jailbreak programs.
* `0x1111111111111111` - used on the unc0ver jailbreak program.

_some more generic nonce generators provided by tihmstar:_
* `0xbd34a880be0b53f3`
* `0x9d0b5b5ff92fff23`
* `0x4bb8834ba6444b50`
* `0x698337f5a79c3292`
* `0xedeeb72d7575e360`

Note: the Nonce collision method isn't needed anymore, because we now got the [checkm8](https://github.com/axi0mx/ipwndfu) Bootrom exploit.

# Dependencies
*  ## Bundled libs
  Those don't need to be installed manually
  * [tss](https://github.com/libimobiledevice)
* ## External libs
  Make sure these are installed
  * [libcurl](https://curl.haxx.se/libcurl/)
  * [libplist](https://github.com/libimobiledevice/libplist)
  * [libfragmentzip](https://github.com/tihmstar/libfragmentzip)
  * [openssl](https://github.com/openssl/openssl) or commonCrypto on macOS/OS X;
  * [libirecovery](https://github.com/libimobiledevice/libirecovery);
* ## Submodules
  Make sure these projects compile on your system
  * [jssy](https://github.com/tihmstar/jssy)

## tsschecker help  
_(might become outdated):_

Usage: `tsschecker [OPTIONS]`

| option (short) | option (long)             | description                                                                       |
|----------------|---------------------------|-----------------------------------------------------------------------------------|
|  `-d`          | `--device MODEL`          | specify device by its MODEL (eg. `iPhone4,1`)                                     |
|  `-i`          | `--ios VERSION`           | specify iOS version (eg. `6.1.3`)                                                 |
|  `-Z`				      | `--buildid BUILDID`	| specific buildid instead of iOS version (eg. `13C75`)							               |
|  `-B` 	   | `--boardconfig BOARD`	   | specific boardconfig instead of iPhone model (eg. `n61ap`)						             |
|  `-h`          | `--help`                  | prints usage information                                                          |
|  `-o`          | `--ota`	                 | check OTA signing status, instead of normal restore                               |
|  `-b`          | `--no-baseband`           | don't check baseband signing status. Request a ticket without baseband            |
|  `-m`          |`--build-manifest MANIFEST`| manually specify buildmanifest (can be used with `-d`)                           | 
|  `-u`          |`--update-install         `| request update ticket instead of erase                          |  
|  `-s`          | `--save`		     		       | save fetched shsh blobs (mostly makes sense with -e)                              |
|  `-l`			     | `--latest`  				       | use latest public iOS version instead of manually specifying one<br>especially useful with `-s` and `-e` for saving blobs                                                                                              |
|      			     | `--apnonce NONCE`   		   | manually specify ApNonce instead of using random one (not required for saving blobs) |
|      			     | `--sepnonce NONCE`        | manually specify SepNonce instead of using random one (not required for saving blobs) 		                                                                                                                                  |
|                           | `--bbsnum SNUM`        | manually specify BbSNUM in HEX for saving valid BBTicket                                                                                                                                         |
|  `-g`                       | `--generator GEN`        | manually specify generator in format 0x%%16llx                                                                                                                                         |
|      			     | `--save-path PATH`        | specify path for saving blobs 		 														 |
|  `-e`          | `--ecid ECID`	         | manually specify an ECID to be used for fetching blobs, instead of using random ones. <br>ECID must be either dec or hex eg. `5482657301265` or `ab46efcbf71`                                                          |
|                |  `--beta`	             | request ticket for beta instead of normal release (use with `-o`)                |
|                | `--list-devices`          | list all known devices                                                            |
|                |`--list-ios`	             | list all known iOS versions                                                       |
|                |`--nocache`       	     | ignore caches and re-download required files                                      |
|                |`--print-tss-request`      | prints TSS request that will be sent to Apple                                      |
|                |`--print-tss-response`     | prints TSS response that come from Apple                                  |
|                |`--raw`     | send raw file to Apple's TSS server (useful for debugging)                                 |
