import sqlite3

def _dict_factory(cursor, row):
    d = {}
    for idx,col in enumerate(cursor.description):
        d[col[0]] = row[idx]
    return d

class ResultData:
    def __init__(self, filename):
        self.db = sqlite3.connect(filename)
        self.db.row_factory = _dict_factory

    def load_last(self, testname, config):
        d = {}
        cur = self.db.cursor()
        cur.execute("SELECT * FROM fio_runs WHERE config = ? AND name = ?ORDER BY time DESC LIMIT 1",
                    (config,testname))
        d['global'] = cur.fetchone()
        if d['global'] is None:
            return None
        cur.execute("SELECT * FROM fio_jobs WHERE run_id = ?",
                    (d['global']['id'],))
        d['jobs'] = cur.fetchall()
        return d

    def _insert_obj(self, tablename, obj):
        keys = obj.keys()
        values = obj.values()
        cur = self.db.cursor()
        cmd = "INSERT INTO {} ({}) VALUES ({}".format(tablename,
                                                       ",".join(keys),
                                                       '?,' * len(values))
        cmd = cmd[:-1] + ')'
        cur.execute(cmd, tuple(values))
        self.db.commit()
        return cur.lastrowid

    def insert_result(self, result):
        row_id = self._insert_obj('fio_runs', result['global'])
        for job in result['jobs']:
            job['run_id'] = row_id
            self._insert_obj('fio_jobs', job)
