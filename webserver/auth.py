from flask import Blueprint, current_app as app,  session, redirect, url_for, request, render_template
from simplepam import authenticate
from functools import wraps


auth = Blueprint('auth', __name__)


@auth.route('/login', methods=['POST'])
def login():
    username = request.form['username']
    password = request.form['password']

    # if login ok, store username to session and redirect to main page
    if authenticate(str(username), str(password)):
        session['username'] = request.form['username']
        return redirect(url_for('index'))

    # if login failed, redirect back to login and show error message
    return render_template("login.html", message='Invalid username or password.')


@auth.route('/logout', methods=['GET', 'POST'])
def logout():
    session['username'] = None
    return redirect(url_for('login'))


# Authentication decorator
def login_required(f):
    @wraps(f)
    def decorator(*args, **kwargs):
        if not session.get('username'):             # don't have username? redirect to login
            return redirect(url_for('login'))

        return f(*args, **kwargs)

    return decorator
