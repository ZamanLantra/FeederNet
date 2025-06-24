import numpy as np
import scipy.stats as si

# https://en.wikipedia.org/wiki/Black%E2%80%93Scholes_model

def black_scholes_price(S, K, T, r, sigma, option_type='call'):
    d1 = (np.log(S / K) + (r + 0.5 * sigma**2) * T) / (sigma * np.sqrt(T))
    d2 = d1 - sigma * np.sqrt(T)
    if option_type == 'call':
        return S * si.norm.cdf(d1) - K * np.exp(-r * T) * si.norm.cdf(d2)
    else:
        return K * np.exp(-r * T) * si.norm.cdf(-d2) - S * si.norm.cdf(-d1)

def calculate_black_scholes_greeks(S, K, T, r, sigma, option_type='call'):
    d1 = (np.log(S / K) + (r + 0.5 * sigma ** 2)*T) / (sigma * np.sqrt(T))
    d2 = d1 - sigma * np.sqrt(T)

    delta = si.norm.cdf(d1) if option_type == 'call' else -si.norm.cdf(-d1)
    gamma = si.norm.pdf(d1) / (S * sigma * np.sqrt(T))
    vega = S * si.norm.pdf(d1) * np.sqrt(T)
    theta = (
        - (S * si.norm.pdf(d1) * sigma) / (2 * np.sqrt(T)) 
        - r * K * np.exp(-r*T) * si.norm.cdf(d2 if option_type == 'call' else -d2)
    ) / 365
    rho = (K * T * np.exp(-r*T) * si.norm.cdf(d2 if option_type == 'call' else -d2)) / 100

    return { 'delta': delta, 'gamma': gamma, 'vega': vega, 'theta': theta, 'rho': rho }

# use below to evaluate black sholes equation within the given strike and maturity range
def calculate_black_scholes_price_grid(S, r, sigma, option_type, strikes, maturities):
    K_grid, T_grid = np.meshgrid(strikes, maturities)
    prices = black_scholes_price(S, K_grid, T_grid, r, sigma, option_type)
    return K_grid, T_grid, prices

