Poddown
=======

A simple podcast downloader.

Poddown is intended for automatically downloading podcasts on a headless
server. Typically this is accomplished with a cron task. It is designed
to provide a minimally rich feature set but is not a full podcast management
application. This only downloads podcasts. Managing the files there after
is outside the scope of this application.


Dependencies
------------

* libcurl
* libxml2


Features
--------

* Partial download resumption
* Check last modified date on the server for the feed to determine if it should
  be downloaded. This is a nice way to reduce bandwidth usage for both ends.
* The ability to filter out podcasts marked as explicit.
* Parallel downloading of feeds and casts. This is configurable.


Download Layout
---------------

Casts can have a name and category assigned which influence the download location.
Both of these are optional and if omitted casts will be downloaded to the top
level of the download location. For example:

* download_root/category/name/cast
* download_root/name/cast
* download_root/cast

