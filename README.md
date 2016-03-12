tsschecker is a powerfull tool to check tss signing status of various devices and ios versions.

It allows you to get lists of all devices and all ios/ota versions for a specific device.

This tool is not only limited to checking versions available for default, since it allows manually specifying a BuildManifest.plist
it can be used to for example check signing status of a beta ipsw.

You do not need to specify any device relevant values to just check status, but if you want you can speficy and ECID for the request.
This combined with --print-tss-response technically allows saving blobs (though there are lot easier way of saving blobs out there).

tsschecker is meant to be used for simply checking signing status, but also for exploring apple's tss servers.
By using all of it's customisazion possibilities you might discover a combination which is signed, which wasn't known before.


tsschecker help (might become outdated):

Usage: tsschecker [OPTIONS]
Checks (real) signing status of device/firmware

  -d, --device MODEL	specific device by its MODEL (eg. iPhone4,1)
  -i, --ios VERSION	specific iOS version (eg. 6.1.3)
  -h, --help		prints usage information
  -o, --ota		check OTA signing status, instead of normal restore
  -b, --no-baseband	don't check baseband signing status. Request a ticket without baseband
  -e, --ecid ECID	manually specify ECID to be used for fetching blobs, instead of using random ones
                 	ECID must be either dec or hex eg. 5482657301265 or ab46efcbf71
      --beta		request ticket for beta instead of normal relase (use with -o)
      --list-devices	list all known devices
      --list-ios	list all known ios versions
      --build-manifest	manually specify buildmanifest. (can be used with -d)
      --nocache       	ignore caches and redownload required files
      --print-tss-request
      --print-tss-response


