import os
from flask import Flask, render_template, abort
from utils import log_config

log_config()
app = Flask(__name__,  template_folder='templates', static_folder='static')


def template_renderer(app):
    def register_template_endpoint(name):
        @app.route('/' + name, endpoint=name)
        def route_handler():
            return render_template(name + '.html')
    return register_template_endpoint


def generate_routes_for_templates():
    app.logger.info("Generating routes for templates (for each worker)")

    web_dir = os.path.dirname(os.path.abspath(__file__))
    web_dir = os.path.join(web_dir, 'templates')
    app.logger.info(f'Will look for templates in dir: {web_dir}')
    register_template_endpoint = template_renderer(app)

    for filename in os.listdir(web_dir):                            # go through templates dir
        full_path = os.path.join(web_dir, filename)                 # construct full path

        fname_wo_ext, ext = os.path.splitext(filename)              # split to filename and extension

        # if it's not a file or doesn't end with htm / html, skip it
        if not os.path.isfile(full_path) or ext not in['.htm', '.html']:    # not a file or not supported extension?
            continue

        app.logger.info(f'Added route /{fname_wo_ext} for template {filename}')
        register_template_endpoint(fname_wo_ext)


generate_routes_for_templates()


@app.route('/{path}')
def template_route(path):
    return render_template(path + '.html')


@app.route('/')
def web_root():
    return render_template("index.html")


@app.route('/status')
def status():
    return "not yet"


@app.route("/site-map")
def site_map():
    links = []
    for rule in app.url_map.iter_rules():
        # url = url_for(rule.endpoint, **(rule.defaults or {}))
        # links.append((url, rule.endpoint))
        links.append(rule.endpoint)

    return links


def render_static_template(name):
    temp_path = os.path.join('static', name)

    if os.path.exists(temp_path):
        return render_template(temp_path)

    if os.path.exists(temp_path + ".html"):
        return render_template(temp_path + ".html")

    abort(404)
