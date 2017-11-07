import json
import argparse
import FioResultDecoder
from dateutil.parser import parse

def is_date(string):
    try:
        parse(string)
        return True
    except ValueError:
        return False

def print_schema_def(key, value, required):
    typestr = value.__class__.__name__
    if typestr == 'str' or typestr == 'unicode':
        if (is_date(value)):
            typestr = "datetime"
        else:
            typestr = "varchar(256)"
    requiredstr = ""
    if required:
        requiredstr = " NOT NULL"
    return ",\n  `{}` {}{}".format(key, typestr, requiredstr)

parser = argparse.ArgumentParser()
parser.add_argument('infile', help="The json file to strip")
args = parser.parse_args()

json_data = open(args.infile)
data = json.load(json_data, cls=FioResultDecoder.FioResultDecoder)

# These get populated by the test runner, not fio, so add them so their
# definitions get populated in the schema properly
data['global']['config'] = 'default'
data['global']['kernel'] = '4.14'
data['global']['name'] = 'alrightalrightalright'

print("CREATE TABLE IF NOT EXISTS `fio_runs` (")
outstr = "  `id` INTEGER PRIMARY KEY AUTOINCREMENT"
for key,value in data['global'].iteritems():
    outstr += print_schema_def(key, value, True)
print(outstr)
print(");")

required_fields = ['run_id']

job = data['jobs'][0]
job['run_id'] = 0

print("CREATE TABLE IF NOT EXISTS `fio_jobs` (")
outstr = "  `id` INTEGER PRIMARY KEY AUTOINCREMENT"
for key,value in job.iteritems():
    outstr += print_schema_def(key, value, key in required_fields)
print(outstr)
print(");")
