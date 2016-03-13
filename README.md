#tsschecker
_tsschecker is a powerful tool to check tss signing status of various devices and ios versions._

It allows you to get lists of all devices and all ios/ota versions for a specific device.

This tool is not only limited to checking versions available for default, since it allows manually specifying a `BuildManifest.plist`  
it can be used to check signing status of a beta ipsw.

You do not need to specify any device relevant values to just check status, but if you want you can specify and ECID for the request.  
This combined with `--print-tss-response` technically allows saving blobs (though there are lot easier way of saving blobs out there).

tsschecker is meant to be used for simply checking signing status, but also for exploring apple's tss servers.
By using all of it's customization possibilities you might discover a combination which is signed, which wasn't known before.  


##tsschecker help  
_(might become outdated):_

Usage: `tsschecker [OPTIONS]`  
_Checks (real) signing status of device/firmware_  

  `-d`, `--device MODEL`	specify device by its MODEL (eg. `iPhone4,1`)  
  `-i`, `--ios VERSION`	specify iOS version (eg. `6.1.3`)  
  `-h`, `--help`		prints usage information  
  `-o`, `--ota`		check OTA signing status, instead of normal restore  
  `-b`, `--no-baseband`	don't check baseband signing status. Request a ticket without baseband  
  `-e`, `--ecid ECID`	manually specify ECID to be used for fetching blobs, instead of using random ones  
                 	ECID must be either dec or hex eg. `5482657301265` or `ab46efcbf71`    
      `--beta`		request ticket for beta instead of normal release (use with `-o`)  
      `--list-devices`	list all known devices  
      `--list-ios`	list all known ios versions  
      `--build-manifest` MANIFEST	manually specify buildmanifest. (can be used with `-d`)  
      `--nocache`       	ignore caches and re-download required files  
      `--print-tss-request`  
      `--print-tss-response`  
