default_keys = [ 'iops', 'io_bytes', 'bw' ]
latency_keys = [ 'lat_ns_min', 'lat_ns_max' ]
main_job_keys = [ 'sys_cpu', 'elapsed' ]
io_ops = ['read', 'write', 'trim' ]

def _fuzzy_compare(a, b, fuzzy):
    if a == b:
        return 0
    if a == 0:
        return 100
    a = float(a)
    b = float(b)
    fuzzy = float(fuzzy)
    val = ((b - a) / a) * 100
    if val > fuzzy or val < -fuzzy:
        return val;
    return 0

def _compare_jobs(ijob, njob, latency, fuzz, failures_only):
    failed = 0
    for k in default_keys:
        for io in io_ops:
            key = "{}_{}".format(io, k)
            comp = _fuzzy_compare(ijob[key], njob[key], fuzz)
            if comp < 0:
                print("    {} regressed: old {} new {} {}%".format(key,
                      ijob[key], njob[key], comp))
                failed += 1
            elif not failures_only and comp > 0:
                print("    {} improved: old {} new {} {}%".format(key,
                      ijob[key], njob[key], comp))
            elif not failures_only:
                print("{} is a-ok {} {}".format(key, ijob[key], njob[key]))
    for k in latency_keys:
        if not latency:
            break
        for io in io_ops:
            key = "{}_{}".format(io, k)
            comp = _fuzzy_compare(ijob[key], njob[key], fuzz)
            if comp > 0:
                print("    {} regressed: old {} new {} {}%".format(key,
                      ijob[key], njob[key], comp))
                failed += 1
            elif not failures_only and comp < 0:
                print("    {} improved: old {} new {} {}%".format(key,
                      ijob[key], njob[key], comp))
            elif not failures_only:
                print("{} is a-ok {} {}".format(key, ijob[key], njob[key]))
    for k in main_job_keys:
        comp = _fuzzy_compare(ijob[k], njob[k], fuzz)
        if comp > 0:
            print("    {} regressed: old {} new {} {}%".format(k, ijob[k],
                  njob[k], comp))
            failed += 1
        elif not failures_only and comp < 0:
            print("    {} improved: old {} new {} {}%".format(k, ijob[k],
                  njob[k], comp))
        elif not failures_only:
                print("{} is a-ok {} {}".format(k, ijob[k], njob[k]))
    return failed

def compare_individual_jobs(initial, data, fuzz, failures_only):
    failed = 0;
    initial_jobs = initial['jobs'][:]
    for njob in data['jobs']:
        for ijob in initial_jobs:
            if njob['jobname'] == ijob['jobname']:
                print("  Checking results for {}".format(njob['jobname']))
                failed += _compare_jobs(ijob, njob, fuzz, failures_only)
                initial_jobs.remove(ijob)
                break
    return failed

def default_merge(data):
    '''Default merge function for multiple jobs in one run

    For runs that include multiple threads we will have a lot of variation
    between the different threads, which makes comparing them to eachother
    across multiple runs less that useful.  Instead merge the jobs into a single
    job.  This function does that by adding up 'iops', 'io_kbytes', and 'bw' for
    read/write/trim in the merged job, and then taking the maximal values of the
    latency numbers.
    '''
    merge_job = {}
    for job in data['jobs']:
        for k in main_job_keys:
            if k not in merge_job:
                merge_job[k] = job[k]
            else:
                merge_job[k] += job[k]
        for io in io_ops:
            for k in default_keys:
                key = "{}_{}".format(io, k)
                if key not in merge_job:
                    merge_job[key] = job[key]
                else:
                    merge_job[key] += job[key]
            for k in latency_keys:
                key = "{}_{}".format(io, k)
                if key not in merge_job:
                    merge_job[key] = job[key]
                elif merge_job[key] < job[key]:
                    merge_job[key] = job[key]
    return merge_job

def compare_fiodata(initial, data, latency, merge_func=default_merge, fuzz=5,
                    failures_only=True):
    failed  = 0
    if merge_func is None:
        return compare_individual_jobs(initial, data, fuzz, failures_only)
    ijob = merge_func(initial)
    njob = merge_func(data)
    return _compare_jobs(ijob, njob, latency, fuzz, failures_only)
