from flask import Flask, render_template
from utils import log_config, generate_routes_for_templates
from stream import stream
from download import download
from floppy import floppy

log_config()
app = Flask(__name__,  template_folder='templates', static_folder='static')
app.register_blueprint(stream, url_prefix='/stream')
app.register_blueprint(download, url_prefix='/download')
app.register_blueprint(floppy, url_prefix='/floppy')

generate_routes_for_templates(app)


@app.route('/', methods=['GET'])
def web_root():
    return render_template("index.html")


@app.route('/status', methods=['GET'])
def status():
    return "not yet"
