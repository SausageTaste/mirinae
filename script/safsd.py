

TRILION = 1000000000000
BILLION = 1000000000
MILLION = 1000000

DATA = [
    ("EU", 20.29 * TRILION, 449.2 * MILLION),
    ("East Asia", 30775.66 * BILLION, 1_653_646_944)
]

for name, gdp, pop in DATA:
    print(f"{name}: {gdp / pop:.2f} per capita")
