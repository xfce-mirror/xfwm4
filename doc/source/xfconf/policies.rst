Window policies
================

Location
--------

Window policies are settings applied to individual windows, based on matching
by window selector:

  <window class type>.<window class name>.<window name>.<window type>

*(window type is the Atom name of the netwm window type property value)*

Each element of this selector may be either the full name (exact match) or
an asterisk (match anything). Matching is done from the most exact to least
(so '\*.*.*.*' matches last). This way, both generic and specific settings can
be applied together (eg. for all windows, for all dialogs, one specific
application, etc).

These policies are stored under the path /policies (array), each of them an
array property with the selector as name and a list of settings that apply
to windows matching this selector.

Keys
----

.. csv-table::
  :file: policies-keys.csv
  :header-rows: 1


Example
-------

.. literalinclude:: policies-example.xml
    :language: xml
