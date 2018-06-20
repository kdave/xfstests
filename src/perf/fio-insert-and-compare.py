# SPDX-License-Identifier: GPL-2.0

import FioResultDecoder
import ResultData
import FioCompare
import json
import argparse
import sys
import platform

parser = argparse.ArgumentParser()
parser.add_argument('-c', '--configname', type=str,
                    help="The config name to save the results under.",
                    required=True)
parser.add_argument('-d', '--db', type=str,
                    help="The db that is being used", required=True)
parser.add_argument('-n', '--testname', type=str,
                    help="The testname for the result", required=True)
parser.add_argument('result', type=str,
                    help="The result file to compare and insert")
args = parser.parse_args()

result_data = ResultData.ResultData(args.db)
compare = result_data.load_last(args.testname, args.configname)

json_data = open(args.result)
data = json.load(json_data, cls=FioResultDecoder.FioResultDecoder)
data['global']['name'] = args.testname
data['global']['config'] = args.configname
data['global']['kernel'] = platform.release()
result_data.insert_result(data)

if compare is None:
    sys.exit(0)

if FioCompare.compare_fiodata(compare, data, False):
    sys.exit(1)
