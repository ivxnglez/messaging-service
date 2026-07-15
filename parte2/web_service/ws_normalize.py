
from fastapi import FastAPI
from pydantic import BaseModel
import uvicorn
import re


class MensajeIn(BaseModel):
    message: str


class MensajeOut(BaseModel):
    normalized: str

# creamos la aplicación FastAPI
app = FastAPI(
    title="Servicio Web de Normalización de Mensajes",
    description="Elimina los espacios en blanco repetidos de un mensaje.",
    version="1.0.0",
)


@app.post("/normalize", response_model=MensajeOut)
async def normalize(msg: MensajeIn):
    # usamos una expresion regular para normalizar la cadena recibida y la retornamos
    normalizado = re.sub(r"\s+", " ", msg.message).strip()
    return {"normalized": normalizado}

@app.get("/")
async def root():
    return {"service": "normalize", "status": "ok"}

# hardcodeamos la ip ya que el enunciado dice que siempre será en local
if __name__ == "__main__":
    uvicorn.run(app, host="127.0.0.1", port=5000)

    