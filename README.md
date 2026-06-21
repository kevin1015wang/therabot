<div align="center">
  <img alt="theracat-gpt-logo" src="https://github.com/user-attachments/assets/9fdde664-2e8a-4728-bfdc-6e741faf23a8" />
</div>

<div align="center">
  <img src="https://github.com/user-attachments/assets/3d8bfb6c-5452-4081-9cfd-a73aecb208ed" width="450">
</div>

## ML Model Demo & Google Colab Notebook
<div align="center">
  <img src="https://github.com/user-attachments/assets/f92592f0-9d48-48d2-9166-e8e89213052c" width="700">
</div>

<div align="center">
  <img alt="SCR-20260621-ksih" src="https://github.com/user-attachments/assets/0f37d528-b592-414d-864d-0601fb56f9c3" />
</div>

<p align="center"><em>
Figure: Example accelerometer signals from TheraCat showing stable motion patterns during a calm state and larger fluctuations during an agitated state used for stress classification. Live accelerometer data visualised in real time.
</em></p>


Figure: Example accelerometer signals from TheraCat showing stable motion patterns during a calm state and larger fluctuations during an agitated state used for stress classification. Live accelerometer data visualised in real time.

TheraCat's stress-detection pipeline is backed by a lightweight accelerometer-based classification model. To make the development process transparent and reproducible, we included a Jupyter notebook that lets you explore the model, visualize sensor signals, and run predictions on sample data.

The Google colab notebook (Therabot-Classifier.ipynb) walks through the full workflow:

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

