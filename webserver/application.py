import os
from flask import Flask, render_template
from utils import log_config, generate_routes_for_templates, text_from_file
from shared import SECRET_KEY_PATH
from stream import stream
from download import download
from floppy import floppy
from auth import auth, login_required
from shared import FLOPPY_UPLOAD_PATH

log_config()
app = Flask(__name__,  template_folder='templates', static_folder='static')
app.secret_key = text_from_file(SECRET_KEY_PATH)        # set the secret key on start

os.makedirs(FLOPPY_UPLOAD_PATH, exist_ok=True)          # create upload folder if it doesn't exist
app.config['UPLOAD_FOLDER'] = FLOPPY_UPLOAD_PATH        # default upload location
app.config['MAX_CONTENT_PATH'] = 5*1024*1024            # max file size

app.register_blueprint(auth, url_prefix='/auth')
app.register_blueprint(stream, url_prefix='/stream')
app.register_blueprint(download, url_prefix='/download')
app.register_blueprint(floppy, url_prefix='/floppy')

generate_routes_for_templates(app)


@app.route('/', methods=['GET'])
@login_required
def web_root():
    return render_template("index.html")


@app.route('/login', methods=['GET'])       # login page is without @login_required
def login():
    return render_template("login.html")


@app.route('/status', methods=['GET'])
@login_required
def status():
    return "not yet"
