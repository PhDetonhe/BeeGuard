"""BeeGuard local result-only API and dashboard server (no audio upload route)."""
from datetime import datetime, timezone
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import json
from urllib.parse import urlparse

ROOT=Path(__file__).resolve().parents[1]
HISTORY=[]
ALLOWED={'HEALTHY','ALERT','PROBLEM'}
FEATURES=('dominant_frequency','db_mean','db_max','rms','energy_100_300','energy_200_270','energy_300_600')

class Handler(SimpleHTTPRequestHandler):
    def __init__(self,*args,**kwargs): super().__init__(*args,directory=str(ROOT/'dashboard'),**kwargs)
    def send_json(self,status,obj):
        raw=json.dumps(obj).encode(); self.send_response(status); self.send_header('Content-Type','application/json'); self.send_header('Content-Length',str(len(raw))); self.send_header('Cache-Control','no-store'); self.end_headers(); self.wfile.write(raw)
    def do_GET(self):
        if urlparse(self.path).path=='/api/classification': return self.send_json(200,{'latest':HISTORY[-1] if HISTORY else None,'history':HISTORY[-30:][::-1]})
        if self.path=='/': self.path='/index.html'
        return super().do_GET()
    def do_POST(self):
        if urlparse(self.path).path!='/api/classification': return self.send_json(404,{'error':'not found'})
        try:
            size=int(self.headers.get('Content-Length','0'))
            if not 0<size<8192: return self.send_json(400,{'error':'invalid request size'})
            data=json.loads(self.rfile.read(size))
            label=data.get('classification',data.get('state'))
            confidence=float(data['confidence'])
            if label not in ALLOWED or not 0<=confidence<=1: raise ValueError('invalid classification or confidence')
            item={'timestamp':datetime.now(timezone.utc).astimezone().isoformat(timespec='seconds'),'classification':label,'confidence':confidence,'device_id':str(data.get('device_id','esp32'))[:64]}
            for key in FEATURES:
                value=data.get(key)
                if value is not None:
                    value=float(value)
                    if not (-1e12<value<1e12): raise ValueError('invalid feature value')
                    item[key]=value
            HISTORY.append(item); del HISTORY[:-200]
            self.send_json(201,{'accepted':True,'timestamp':item['timestamp']})
        except (ValueError,KeyError,TypeError,json.JSONDecodeError) as exc: self.send_json(400,{'error':str(exc)})

if __name__=='__main__':
    import argparse
    p=argparse.ArgumentParser(); p.add_argument('--host',default='0.0.0.0'); p.add_argument('--port',type=int,default=8000); a=p.parse_args()
    print(f'BeeGuard: http://localhost:{a.port} (results only; in-memory history)')
    ThreadingHTTPServer((a.host,a.port),Handler).serve_forever()
