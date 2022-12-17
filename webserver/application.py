from flask import Flask, render_template

app = Flask(__name__)


@app.route('/')
def index():
    return render_template("index.html")

@app.route('/floppy')
def floppy():
    return render_template("floppy.html")


@app.route('/download')
def download():
    return render_template("download.html")


@app.route('/remote')
def remote():
    return render_template("remote.html")


@app.route('/screenshots')
def screenshots():
    return render_template("screenshots.html")


@app.route('/config')
def config():
    return render_template("config.html")


@app.route('/status')
def status():
    return "not yet"

