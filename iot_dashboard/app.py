from flask import Flask, request, jsonify, render_template, redirect, url_for, flash
from flask_sqlalchemy import SQLAlchemy
from flask_cors import CORS
from flask_login import LoginManager, UserMixin, login_user, login_required, logout_user, current_user
from werkzeug.security import generate_password_hash, check_password_hash
from itsdangerous import URLSafeTimedSerializer
import datetime
from datetime import timezone, timedelta  # <--- NEW
import smtplib
import threading # <--- NEW: Added threading library
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart

app = Flask(__name__)
CORS(app)

# =================================================================
# --- CONFIGURATION ---
# =================================================================
app.secret_key = 'super_secret_key_change_this'
serializer = URLSafeTimedSerializer(app.secret_key)

# SERVER EMAIL SETTINGS 
SENDER_EMAIL = "bhargavshivamcr7@gmail.com"        # <--- PUT SERVER EMAIL HERE
SENDER_PASSWORD = "cxoqdwzulsuyzqjj"   # <--- PUT 16-CHAR APP PASSWORD HERE
# =================================================================

app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///sensor_data.db'
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
db = SQLAlchemy(app)

login_manager = LoginManager()
login_manager.init_app(app)
login_manager.login_view = 'login'

# =================================================================
# --- DATABASE MODELS ---
# =================================================================

class User(UserMixin, db.Model):
    id = db.Column(db.Integer, primary_key=True)
    username = db.Column(db.String(150), unique=True, nullable=False)
    email = db.Column(db.String(150), unique=True, nullable=False) 
    password = db.Column(db.String(150), nullable=False)
    
    # --- ALARM SETTINGS ---
    alert_email = db.Column(db.String(150), nullable=True) 
    alert_phone = db.Column(db.String(20), nullable=True) 
    
    temp_warning = db.Column(db.Float, default=35.0)
    temp_hazard = db.Column(db.Float, default=45.0)
    gas_warning = db.Column(db.Integer, default=200)
    gas_hazard = db.Column(db.Integer, default=300)
    
    devices = db.relationship('Device', backref='owner', lazy=True)

class Device(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    hardware_id = db.Column(db.String(50), unique=True, nullable=False)
    friendly_name = db.Column(db.String(50), nullable=False)
    user_id = db.Column(db.Integer, db.ForeignKey('user.id'), nullable=False)
    readings = db.relationship('SensorData', backref='device', lazy=True)

# --- NEW: Helper function for Indian Standard Time (UTC +5:30) ---
def get_local_time():
    ist = timezone(timedelta(hours=5, minutes=30))
    return datetime.datetime.now(ist)

class SensorData(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    device_db_id = db.Column(db.Integer, db.ForeignKey('device.id'), nullable=False)
    temperature = db.Column(db.Float, nullable=False)
    humidity = db.Column(db.Float, nullable=False)
    gas_level = db.Column(db.Integer, nullable=False)
    
    # --- UPDATED: Use the local time function instead of utcnow ---
    timestamp = db.Column(db.DateTime, default=get_local_time)

    def to_dict(self):
        return {
            'temperature': self.temperature,
            'humidity': self.humidity,
            'gas_level': self.gas_level,
            # Format the time nicely before sending it to the dashboard
            'timestamp': self.timestamp.strftime("%Y-%m-%d %I:%M:%S %p") 
        }

@login_manager.user_loader
def load_user(user_id):
    return User.query.get(int(user_id))

# =================================================================
# --- ALERT HELPERS (EMAIL & SMS) ---
# =================================================================

# --- NEW: The actual email logic moved to a background worker function
def _async_send_email(receiver_email, subject, body):
    if not receiver_email: return
    try:
        msg = MIMEMultipart()
        msg['From'] = SENDER_EMAIL
        msg['To'] = receiver_email
        msg['Subject'] = subject
        msg.attach(MIMEText(body, 'plain'))
        
        # Added a 10-second timeout so it will fail gracefully instead of freezing
        server = smtplib.SMTP('smtp.gmail.com', 587, timeout=10)
        server.starttls() 
        server.login(SENDER_EMAIL, SENDER_PASSWORD)
        server.sendmail(SENDER_EMAIL, receiver_email, msg.as_string())
        server.quit()
        print(f"[*] Email sent to {receiver_email}")
    except Exception as e:
        print(f"[!] Failed to send email: {e}")

def send_email(receiver_email, subject, body):
    # --- NEW: Spawns the background thread and instantly returns control to the server
    thread = threading.Thread(target=_async_send_email, args=(receiver_email, subject, body))
    thread.start()

def send_sms(phone_number, body):
    if not phone_number: return
    # NOTE: To send REAL text messages, you need to use an API like Twilio.
    # For now, we simulate it by printing to the terminal.
    print(f"\n{'='*40}")
    print(f"📱 SIMULATED SMS TO {phone_number}")
    print(f"Message: {body}")
    print(f"{'='*40}\n")

def dispatch_alerts(user, subject, body):
    """Sends both Email and SMS if the user has them configured."""
    if user.alert_email:
        send_email(user.alert_email, subject, body)
    if user.alert_phone:
        send_sms(user.alert_phone, f"{subject} - {body}")

# =================================================================
# --- AUTH & GENERAL ROUTES ---
# =================================================================

@app.route('/signup', methods=['GET', 'POST'])
def signup():
    if request.method == 'POST':
        username = request.form.get('username')
        email = request.form.get('email')
        password = request.form.get('password')
        if User.query.filter_by(email=email).first() or User.query.filter_by(username=username).first():
            flash('Username or Email already exists.')
            return redirect(url_for('signup'))
        
        new_user = User(username=username, email=email, alert_email=email, password=generate_password_hash(password))
        db.session.add(new_user)
        db.session.commit()
        login_user(new_user)
        return redirect(url_for('dashboard'))
    return render_template('signup.html')

@app.route('/login', methods=['GET', 'POST'])
def login():
    if request.method == 'POST':
        user = User.query.filter_by(email=request.form.get('email')).first()
        if user and check_password_hash(user.password, request.form.get('password')):
            login_user(user)
            return redirect(url_for('dashboard'))
        flash('Invalid email or password.')
    return render_template('login.html')

@app.route('/logout')
@login_required
def logout():
    logout_user()
    return redirect(url_for('login'))

# =================================================================
# --- DASHBOARD & SETTINGS ---
# =================================================================

@app.route('/')
@login_required
def dashboard():
    return render_template('dashboard.html', user=current_user)

@app.route('/settings', methods=['GET', 'POST'])
@login_required
def settings():
    if request.method == 'POST':
        new_password = request.form.get('new_password')
        if new_password:
            current_user.password = generate_password_hash(new_password)
            db.session.commit()
            flash('Password updated successfully!', 'success')
        return redirect(url_for('settings'))
    return render_template('settings.html', user=current_user)

@app.route('/alarm_settings', methods=['GET', 'POST'])
@login_required
def alarm_settings():
    if request.method == 'POST':
        current_user.alert_email = request.form.get('alert_email')
        current_user.alert_phone = request.form.get('alert_phone')
        
        # Parse thresholds safely
        try:
            current_user.temp_warning = float(request.form.get('temp_warning', 35.0))
            current_user.temp_hazard = float(request.form.get('temp_hazard', 45.0))
            current_user.gas_warning = int(request.form.get('gas_warning', 200))
            current_user.gas_hazard = int(request.form.get('gas_hazard', 300))
            db.session.commit()
            flash('Alarm thresholds updated successfully!', 'success')
        except ValueError:
            flash('Invalid input for thresholds. Please use numbers.', 'error')
            
        return redirect(url_for('alarm_settings'))
    return render_template('alarm_settings.html', user=current_user)

@app.route('/add_device', methods=['POST'])
@login_required
def add_device():
    hw_id = request.form.get('hardware_id')
    if Device.query.filter_by(hardware_id=hw_id).first():
        flash('Device already registered!')
    else:
        new_device = Device(hardware_id=hw_id, friendly_name=request.form.get('friendly_name'), owner=current_user)
        db.session.add(new_device)
        db.session.commit()
    return redirect(url_for('dashboard'))

# =================================================================
# --- API ROUTES (SMART ALARMS INCLUDED) ---
# =================================================================

@app.route('/api/post_data', methods=['POST'])
def post_data():
    data = request.get_json()
    if not data: return jsonify({"error": "No data"}), 400
    
    device = Device.query.filter_by(hardware_id=data.get('device_id')).first()
    if not device: return jsonify({"error": "Device not registered"}), 404
        
    temp = data.get('temperature')
    hum = data.get('humidity')
    gas = data.get('gas_level')
    
    owner = device.owner
    room = device.friendly_name

    # --- 1. EVALUATE GAS ---
    if gas is not None:
        if gas >= owner.gas_hazard:
            dispatch_alerts(owner, f"🔴 HAZARD: Critical Gas in {room}!", f"Level: {gas}. Evacuate and check immediately!")
        elif gas >= owner.gas_warning:
            dispatch_alerts(owner, f"🟠 WARNING: Poor Air Quality in {room}", f"Level: {gas}. Please ventilate the area.")

    # --- 2. EVALUATE TEMPERATURE ---
    if temp is not None:
        if temp >= owner.temp_hazard:
            dispatch_alerts(owner, f"🔴 HAZARD: Extreme Heat in {room}!", f"Temp: {temp}°C. Fire or extreme overheat risk!")
        elif temp >= owner.temp_warning:
            dispatch_alerts(owner, f"🟠 WARNING: High Temp in {room}", f"Temp: {temp}°C. Cooling may be required.")

    # Save to DB
    new_reading = SensorData(device=device, temperature=temp, humidity=hum, gas_level=gas)
    db.session.add(new_reading)
    db.session.commit()
    return jsonify({"message": "Saved"}), 201

@app.route('/api/get_data/<int:device_db_id>', methods=['GET'])
@login_required
def get_data(device_db_id):
    device = Device.query.get_or_404(device_db_id)
    if device.owner != current_user: return jsonify({"error": "Unauthorized"}), 403
    readings = SensorData.query.filter_by(device_db_id=device.id).order_by(SensorData.timestamp.desc()).limit(20).all()
    return jsonify([r.to_dict() for r in readings])

if __name__ == '__main__':
    with app.app_context():
        db.create_all()
    app.run(host='0.0.0.0', port=5000, debug=True)