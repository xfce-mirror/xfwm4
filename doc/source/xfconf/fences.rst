Window fences
=============

Window fences are (usually invisible) screen areas where windows expand into,
instead of the whole monitor, when being within a fence. A window is considered
in a fence, when overlapping with it (on overlap w/ more than one, the biggest
overlap wins).

On movement, when the window is over a fence, the fence's frame is shown.

The current window fence is stored in the window propert "XFWM_FENCE" and
can be changed by clients via property update.

Location
--------

Window fences are defined in xfwm's xfconf channel under path '/fences'.

Each fence is defined in an 'array' holding several settings, eg. geometry,
colors, etc.

Properties
----------

.. csv-table::
  :file: fences-keys.csv
  :header-rows: 1


Example
-------

.. literalinclude:: fences-example.xml
    :language: xml
