<img width="5712" height="4284" alt="IMG_6583" src="https://github.com/user-attachments/assets/3d8bfb6c-5452-4081-9cfd-a73aecb208ed" />

## ML Model Demo & Jupyter Notebook

<img width="1610" height="1042" alt="Screenshot 2026-06-21 at 10 40 31 AM" src="https://github.com/user-attachments/assets/f92592f0-9d48-48d2-9166-e8e89213052c" />

Figure: Example accelerometer signals from TheraCat showing stable motion patterns during a calm state and larger fluctuations during an agitated state used for stress classification.

TheraCat's stress-detection pipeline is backed by a lightweight accelerometer-based classification model. To make the development process transparent and reproducible, we included a Jupyter notebook that lets you explore the model, visualize sensor signals, and run predictions on sample data.

The notebook walks through the full workflow:

- Loading accelerometer data from the MPU6050 sensor
- Preprocessing and cleaning raw motion signals
- Extracting features from X, Y, and Z accelerometer axes
- Running stress/agitation classification
- Visualizing calm versus agitated movement patterns
- Evaluating model output

You can run the notebook locally and experiment with your own sensor data.

### Running the notebook

Clone the repository:

```bash
git clone https://github.com/kevin1015wang/therabot.git
cd therabot

