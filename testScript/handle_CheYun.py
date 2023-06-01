from flask import Flask, request,Response
from flask_restful import Api,Resource
import time
import json
import requests
import  sqlite3
import os

hostName = "0.0.0.0"
serverPort  = 18080
led_server="http://nps.hyman.store:11007/neima?key="
led_server_empty_plot="http://nps.hyman.store:11007/empty_plot"

last_update_response =""
current_empty_plot =0

dbfilePath="file:cheyun.db"

app = Flask(__name__)
app.config['UPLOAD_FOLDER'] = './'

def create_database():
    # Connect to database (creates a new database if it doesn't exist)
    conn = sqlite3.connect(dbfilePath,uri=True)

    # Create the "leds" table with columns "ledid" and "parkid"
    conn.execute('''CREATE TABLE leds (
    ledid TEXT PRIMARY KEY NOT NULL,
    park_id integer NOT NULL    
);''')

    # Create the "parkinfo" table with columns "parkid", "id", and "pgmfilepath"
    conn.execute('''CREATE TABLE parkinfo
                     (park_id integer PRIMARY KEY  NOT NULL,
                    park_name TEXT NOT NULL,
                      pgmfilepath TEXT NOT NULL);''')

    # Commit the changes and close the connection

    conn.execute('''INSERT INTO leds
                    (ledid, park_id)
                    VALUES('960302311001506', 25082);''')
    
    conn.execute('''INSERT INTO leds
                    (ledid, park_id)
                    VALUES('test_abcv12345', 25082);''')
    
    conn.execute('''
                    INSERT INTO parkinfo
                    (park_id, park_name, pgmfilepath)
                    VALUES(25082,'test孵化园', 'single_area.lsprj');''')
    conn.commit()
    conn.close()

def try_openDB():
    try:
        conn = sqlite3.connect(f'{dbfilePath}?mode=ro',uri=True)
        c= conn.cursor()
        c.execute('''select park_id,pgmfilepath from parkinfo;''')
        c.execute('''select ledid,park_id from leds;''')
        conn.close()
    except Exception as e:
        print(e)
        create_database()
    


@app.route('/listdb', methods=['GET'])    
def handle_listdb(): 
    park_id=request.args.get("park_id")
    try:
        conn = sqlite3.connect(f'{dbfilePath}?mode=ro',uri=True)
        c = conn.cursor()
        #sql = f'select ledid,a.park_id,b.park_name ,b.pgmfilepath from leds a,parkinfo b where a.park_id=b.park_id '
        sql = 'select ledid ,park_id from leds '
        if park_id is not None and park_id!="":
            sql += f'and park_id={park_id}'
        c.execute(sql)
        response=f'{park_id}<br>'
        for row in c.fetchall():
            response +=str(row)+'--->'
            sql2='select park_id ,park_name,pgmfilepath from parkinfo where park_id='+str(row[1])
            c.execute(sql2)
            for row2 in c.fetchall():
                response +=str(row2)
                break
            response+='<br>'
        conn.close()
        return response
    except Exception as e:
        print(e)
        return str(e)

@app.route('/test', methods=['GET'])
@app.route('/', methods=['GET'])
def handle_root():
        global last_update_response
        global current_empty_plot

        if request.path=="/test":
            current_empty_plot=int(time.time())
        
        a=f"<p>last_update_response: {last_update_response}</p>"
        a+=f"<p>Current empty plot: {current_empty_plot}</p>"
        return a
        
def handle_park(park_id,empty_plot):
    global last_update_response
    global current_empty_plot
    
    current_empty_plot = empty_plot
    try:
        conn = sqlite3.connect(f'{dbfilePath}?mode=ro',uri=True)
        c = conn.cursor()
        sql = f'select ledid,a.park_id,b.park_name ,b.pgmfilepath from leds a,parkinfo b where a.park_id=b.park_id '
        if park_id!="":
            sql += f'and a.park_id={park_id}'
        print(sql)
        c.execute(sql)
        pgmfilepath=""
        ledids=""
        
        for row in c.fetchall():
            print(row)           
            ledids += str(row[0])+","
            pgmfilepath = str(row[3])
        conn.close()
        url = f'{led_server_empty_plot}?ledids={ledids}&empty_plot={empty_plot}&pgmfilepath={pgmfilepath}&park_id={park_id}'
        print(url)
        response = requests.get(url)
        print(response.text)
        ajson = json.loads(response.text)
        print(ajson)
        print(ajson['idlist'])
        for a in ajson['idlist']:
            print(a)
            if a in ledids:
                continue
            print(f"{a} no in {ledids}")
            requests.post(f'http://127.0.0.1:{serverPort}/newled',json={'ledid':a,'park_id':-1})
        last_update_response = response.text            
        

    except Exception as e:
        print(e)
        return str(e)




@app.route('/out_park', methods=['POST'])  
@app.route('/in_park', methods=['POST']) 
def out_in_park():    
    json_body = request.json
    
    try:
       # json_body = json.loads(body.decode('utf-8'))
        current_empty_plot= json_body['data']['empty_plot']    
        park_id=json_body['park_id']

        handle_park(park_id,current_empty_plot)   
        reuslt={
  "state": 1,
  "order_id": json_body['data']['order_id'],
  "park_id": park_id,
  "service_name": json_body['service_name'],
  "errmsg": " send success!"
}
        return json.dumps(reuslt)
    except Exception as e:
        print("Error parsing JSON body: ", e)
        return str(e)

@app.route('/all', methods=['GET'])
def handle_led_infos():
    try:            
        conn = sqlite3.connect(f'{dbfilePath}?mode=ro',uri=True)
        c = conn.cursor()
        c.execute('''select ledid,park_id from leds;''')        
        response=f'<section> <section><div><h1>LEDs</h1><ul>'
        for row in c.fetchall():
            formHtml=f'''<input type="submit" value="delete" onclick="deleteLed({row[0]})">'''
            response +='<li>'+formHtml+str(row)+'</li>'
        response+='</ul></div></section>'
        response+='<section><div>" " </div></section>'
        c.execute('''select park_id,park_name,pgmfilepath from parkinfo;''')
        response+='<section><div><h1>Parks</h1><ul>'
        for row in c.fetchall():  
            formHtml=f'''<input type="submit" value="delete" onclick="deletepark({row[0]})">'''          
            response +='<li>'+formHtml+str(row)+'</li>'
        response+='</ul></div></section></section>'

        conn.commit()
        conn.close()
        rHtml='''            
        <html>
        <script>
        function setLedAction(form){
            form.action = "/api/ledinfo/"+form.ledid.value;
            return true;
        }
         function setparkAction(form){
                form.action = "/api/parkinfo/"+form.park_id.value;
                return true;
            }       
        function deleteLed(ledid){
            let deleteurl = "/api/ledinfo/"+ledid;
            fetch(deleteurl, {method: "DELETE"});
        }
        function deletepark(parkid){
            let deleteurl = "/api/parkinfo/"+parkid;
            fetch(deleteurl, {method: "DELETE"});
        }

        </script>
        <link rel="stylesheet" href="https://unpkg.com/mvp.css@1.12/mvp.css"> 
        <body>
        <section>
        <section>
        <div><h1>ADD LEDs </h1>
        <form  onsubmit = "return setLedAction(this)" method="POST">	
        <p>ledid: <input type = "text" name = "ledid" />
        <p>Park id: <input type = "text" name = "park_id" />
        <p><input type = "submit" value = "Add" />
        </form>
        </div>
        </section>
        <section><div>" "</div></section>
        <section>
        <div><h1>ADD Parks </h1>
        <form enctype = "multipart/form-data" onsubmit = "return setparkAction(this)" method="POST">	
            <p>Park Name: <input type = "text" name = "park_name" /></p>
            <p>Park id: <input type = "text" name = "park_id" /></p>
            <p>pgm File: <input type = "file" name = "file" /></p>	
            <p><input type = "submit" value = "Add" /></p>
            </form>
            </div>
        </section>
        </section>
        ''';

        rHtml+=f'''<p>{response}</p>            </body>            </html>'''
        resp = Response(rHtml,mimetype='text/html')
        return resp
    except Exception as e:
        print(e)
        return str(e)
    
class LedInfo(Resource):
    def get(self,led_id):
        try:            
            conn = sqlite3.connect(f'{dbfilePath}?mode=ro',uri=True)
            c = conn.cursor()
            c.execute(f'''select ledid,park_id from leds where ledid={led_id};''')
        
            response=f''
            for row in c.fetchall():
                response +=str(row)+','
            conn.commit()
            conn.close()
            return response
        except Exception as e:
            print(e)
            return str(e)
    
    def post(self,led_id):
        try:            
            conn = sqlite3.connect(f'{dbfilePath}',uri=True)
            c = conn.cursor()
            c.execute(f'''insert into leds(park_id , ledid) values({request.form['park_id']},{led_id});''')
        
           
            conn.commit()
            conn.close()
            return "ok"
        except Exception as e:
            print(e)
            return str(e)
    
    def delete(self,led_id):
        try:            
            conn = sqlite3.connect(f'{dbfilePath}',uri=True)
            c = conn.cursor()
            c.execute(f'''delete from leds where ledid={led_id};''')
            conn.commit()
            conn.close()
            return "ok"
        except Exception as e:
            print(e)
            return str(e)

class ParkInfo(Resource):
    def get(self,park_id):
        try:            
            conn = sqlite3.connect(f'{dbfilePath}?mode=ro',uri=True)
            c = conn.cursor()
            c.execute(f'''select park_id,pgmfilepath,park_name from parkinfo where park_id={park_id};''')
        
            response=f'{park_id}<br>'
            for row in c.fetchall():
                response +=str(row)+'--->'
            conn.commit()
            conn.close()
            return response
        except Exception as e:
            print(e)
            return str(e)
        

    def post(self,park_id):
        try:
            file = request.files['file']
            if not file:
                return "no file"
            
            park_name= request.values['park_name']
            lsprjfile = os.path.join(app.config['UPLOAD_FOLDER'], f'{park_id}.lsprj')
            file.save(lsprjfile)                        
            conn = sqlite3.connect(f'{dbfilePath}',uri=True)
            c = conn.cursor()
            c.execute('''insert into parkinfo(park_id,pgmfilepath,park_name) values(?,?,?)''',(park_id,lsprjfile,park_name))
            conn.commit()
            conn.close()
            return "ok"
        except Exception as e:
            print(e)
            return str(e)
    def delete(self,park_id):
        pass


api = Api(app)
api.add_resource(LedInfo, '/api/ledinfo/<int:led_id>')
api.add_resource(ParkInfo, '/api/parkinfo/<int:park_id>')

if __name__ == "__main__":        
    try_openDB()
    app.run(host=hostName, port=serverPort)    
    print("Server started http://%s:%s" % (hostName, serverPort))
    
    print("Server stopped.")