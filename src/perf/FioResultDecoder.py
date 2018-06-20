# SPDX-License-Identifier: GPL-2.0

import json

class FioResultDecoder(json.JSONDecoder):
    """Decoder for decoding fio result json to an object for our database

    This decodes the json output from fio into an object that can be directly
    inserted into our database.  This just strips out the fields we don't care
    about and collapses the read/write/trim classes into a flat value structure
    inside of the jobs object.

    For example
        "write" : {
            "io_bytes" : 313360384,
            "bw" : 1016,
        }

    Get's collapsed to

        "write_io_bytes" : 313360384,
        "write_bw": 1016,

    Currently any dict under 'jobs' get's dropped, with the exception of 'read',
    'write', and 'trim'.  For those sub sections we drop any dict's under those.

    Attempt to keep this as generic as possible, we don't want to break every
    time fio changes it's json output format.
    """
    _ignore_types = ['dict', 'list']
    _override_keys = ['lat_ns', 'lat']
    _io_ops = ['read', 'write', 'trim']

    _transform_keys = { 'lat': 'lat_ns' }

    def decode(self, json_string):
        """This does the dirty work of converting everything"""
        default_obj = super(FioResultDecoder, self).decode(json_string)
        obj = {}
        obj['global'] = {}
        obj['global']['time'] = default_obj['time']
        obj['jobs'] = []
        for job in default_obj['jobs']:
            new_job = {}
            for key,value in job.iteritems():
                if key not in self._io_ops:
                    if value.__class__.__name__ in self._ignore_types:
                        continue
                    new_job[key] = value
                    continue
                for k,v in value.iteritems():
                    if k in self._override_keys:
                        if k in self._transform_keys:
                            k = self._transform_keys[k]
                        for subk,subv in v.iteritems():
                            collapsed_key = "{}_{}_{}".format(key, k, subk)
                            new_job[collapsed_key] = subv
                        continue
                    if v.__class__.__name__ in self._ignore_types:
                        continue
                    collapsed_key = "{}_{}".format(key, k)
                    new_job[collapsed_key] = v
            obj['jobs'].append(new_job)
        return obj
