from flask import Flask, request, jsonify, render_template
from flask_sqlalchemy import SQLAlchemy
from flask_cors import CORS
import datetime

# Initialize the Flask App
app = Flask(__name__)
CORS(app)

# Configure the SQLite database
# The database file will be created in the same directory
app.config['SQLALCHEMY_DATABASE_URI'] = 'sqlite:///sensor_data.db'
app.config['SQLALCHEMY_TRACK_MODIFICATIONS'] = False
db = SQLAlchemy(app)

# --- Database Model ---
# This class defines the structure of our database table
class SensorData(db.Model):
    id = db.Column(db.Integer, primary_key=True)
    temperature = db.Column(db.Float, nullable=False)
    humidity = db.Column(db.Float, nullable=False)
    gas_level = db.Column(db.Integer, nullable=False)
    timestamp = db.Column(db.DateTime, default=datetime.datetime.utcnow)

    def to_dict(self):
        """Converts the object to a dictionary for JSON serialization."""
        return {
            'id': self.id,
            'temperature': self.temperature,
            'humidity': self.humidity,
            'gas_level': self.gas_level,
            'timestamp': self.timestamp.isoformat()
        }

# --- API Routes ---

@app.route('/')
def index():
    """Serves the main HTML dashboard page."""
    return render_template('index.html')

@app.route('/data', methods=['GET', 'POST'])
def handle_data():
    """Handles receiving data from NodeMCU and sending data to the frontend."""
    if request.method == 'POST':
        # This is for the NodeMCU to send data
        data = request.get_json()
        if not data:
            return jsonify({"error": "Invalid data"}), 400

        # Create a new entry in our database
        new_data = SensorData(
            temperature=data.get('temperature'),
            humidity=data.get('humidity'),
            gas_level=data.get('gas')
        )
        db.session.add(new_data)
        db.session.commit()
        return jsonify({"message": "Data received successfully!"}), 201

    elif request.method == 'GET':
        # This is for our website to fetch data
        # Get the last 20 records, ordered by the most recent
        latest_data = SensorData.query.order_by(SensorData.timestamp.desc()).limit(20).all()
        return jsonify([d.to_dict() for d in latest_data])

# --- Main Execution ---
if __name__ == '__main__':
    # This command creates the database table if it doesn't exist
    with app.app_context():
        db.create_all()
    # Run the Flask app
    # host='0.0.0.0' makes it accessible on your local network
    app.run(host='0.0.0.0', port=5000, debug=True)
