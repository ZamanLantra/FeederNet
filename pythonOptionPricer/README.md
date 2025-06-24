# OptionPricer -- Python Flask

Black sholes equation based Option Pricer. 
This produces a heatmap for the configurations `Spot Price`, `Volatility`, `Rate`, `Type (Call/Put)` for the `Strke` and `Maturity` ranges configured. 

## Idea

Use Warwick CS139 - Web Development Technologies techniques to host a web server using flask and plot the heatmap through javascript (using Plotly).

## Getting Started

1. Create a python virtual environment and activate. `python3 -m venv venv` and `source venv/bin/activate `
2. Install the required libraries. `pip3 install -r requirements.txt`
3. Run server. `FLASK_APP=OptionPricer.py flask run`
4. Open in browser through a link similar to `http://127.0.0.1:5000`

## Future work

Calculate for realtime price-updates.