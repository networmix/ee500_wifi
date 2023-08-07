# ee500_wifi
This work was done by me (Andrey Golovanov) as the part of the assignment for EE500 at DCU in 2023 while studying for my second MEng degree.

The goal of this assignment was to study the performance of data transmission in a wireless network environment via modelling and simulations using Network Simulator NS-3. Basically, simulate 802.11 in a set of simple scenarios.

I've refactored the original code of the assignment to make it more readable and added some features to make it more flexible. Published the code here to make it available to the fellow students. Please, feel free to use it as a reference for your own work (but don't copy it, it's not cool).

## Structure of the repository
```
├── ns3_30
│   ├── ee500_wifi.ipynb    <-- the interactive notebook to run the analysis
│   ├── ee500_wifi_app.cc   <-- implementation of Receiver, Sender and TimestampTag
│   ├── ee500_wifi_app.h    <-- headers for Receiver, Sender and TimestampTag
│   ├── ee500_wifi_data.cc  <-- implementation of LocalDataOutput
│   ├── ee500_wifi_data.h   <-- headers for LocalDataOutput
│   ├── ee500_wifi_sim.cc   <-- the main simulation script
│   ├── run.sh              <-- the script to run the simulation
│   └── wifi.sh             <-- the script to run the simulation batches
```

## Running the simulation

There are two scripts to run the simulation: `run.sh` and `wifi.sh`. The first one is used to run a single simulation, the second one is typically used to run a batch of simulations.

To run the simulation, you need:
1. Have NS-3 of the correct version installed.
2. Put the `ns3_30` folder into the `scratch` folder of your NS-3 installation.
3. Adjust `NS3DIR` variable in the `run.sh` and `wifi.sh` scripts to point to your NS-3 installation.
4. Run the simulation using the `run.sh` or `wifi.sh` script.


Here are multiple ways to run the simulation:
```bash
./run.sh --distances=40,50 --staNum=2 --duration=30 --desiredDataRate=2000  # there will be 2 nodes: at 40 and 50 meters respectively

./run.sh --distances=40,50 --staNum=10 --distance=65 --duration=30 --desiredDataRate=2000 --standard=n24  # there will be 10 nodes the first two will be at 40 and 50 meters, the rest will be at 65 meters

./run.sh --distance=40 --staNum=2 --duration=30 --desiredDataRate=2000 --rateControl=constant --phyRate=VhtMcs0

./run.sh --distance=40 --staNum=2 --duration=30 --desiredDataRate=2000 --rateControl=constant --phyRate=VhtMcs0 --TxPowerStart=20 --TxPowerEnd=20

./wifi.sh --input_name1=staNum --input1="1 5 10 15 20" --input_name2=distance --input2="0 5 10 15 20 25 30" --duration=5 --desiredDataRate=1000 --strategy=wifi-radial

./wifi.sh --input_name1=distance --input1="31 32 33 34 35 36 37 38 39 40" --duration=5 --staNum=5 --desiredDataRate=1000 --rateControl=constant

./wifi.sh --input_name1=distance --input1="10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 28 29 30" --duration=5 --staNum=5 --desiredDataRate=1000 --TxPowerStart=20 --TxPowerEnd=20

./wifi.sh --input_name1=distance --input1="14.98 14.99 15 15.01 15.02" --duration=5 --staNum=5 --desiredDataRate=1000 --TxPowerEnd=20 --TxPowerLevels=5

./wifi.sh --input_name1=desiredDataRate --input1="2500 5000 7500" --duration=5 --staNum=10 --distance=10
```

## Running the analysis

The analysis is done in the `ee500_wifi.ipynb` notebook. It's a Jupyter notebook, so you need to have Jupyter installed to run it. The easiest way to run it, at least for me, is to install Jupyter extensions for VS Code and run it from there.

You will also need to install NumPy, Pandas, Matplotlib and Seaborn to run the notebook. The easiest way is to do it from the notebook itself: `!pip3 install numpy pandas matplotlib seaborn`.

The analysis is interactive, so you should wrangle the data in the notebook to get the results you want. The notebook has comments and explanations for the metrics used in the analysis. Play with it and have fun!