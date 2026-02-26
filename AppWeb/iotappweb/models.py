from django.db import models

# Create your models here.

class m1Sensoresdata(models.Model):
    registro = models.BigAutoField(primary_key=True)
    timestamp = models.DateTimeField(auto_now_add=True)
    planta_id = models.CharField(max_length=50)
    temperatura = models.FloatField()
    humedad_aire = models.FloatField()
    humedad_suelo = models.FloatField()
    peso = models.FloatField()

    def __str__(self):
        return f"Reg: {self.registro} - {self.timestamp} - {self.planta_id} - {self.temperatura}°C / {self.humedad_aire}% - {self.humedad_suelo}% - {self.peso}kg"

class m2Sensoresdata(models.Model):
    registro = models.BigAutoField(primary_key=True)
    timestamp = models.DateTimeField(auto_now_add=True)
    tanque_id = models.CharField(max_length=50)
    temperatura = models.FloatField()
    caudal = models.FloatField()
    porcentaje_llenado = models.FloatField()
    nivel = models.FloatField()

    def __str__(self):
        return f"Reg: {self.registro} - {self.timestamp} - {self.tanque_id} - {self.temperatura}°C / {self.caudal}L/min - {self.porcentaje_llenado}%  - {self.nivel}cm"
    
class riegoRegistro(models.Model):
    registro = models.BigAutoField(primary_key=True)
    fecha = models.DateTimeField(auto_now_add=True)
    zona = models.IntegerField()
    accion = models.CharField(max_length=10)  # ON / OFF
    tiempo_segundos = models.IntegerField()
    planta_reg = models.ForeignKey(m1Sensoresdata, on_delete=models.SET_NULL, null=True)
    tanque_reg = models.ForeignKey(m2Sensoresdata, on_delete=models.SET_NULL, null=True)
    modo = models.CharField(max_length=15)

    class Meta:
        ordering = ['-fecha']

    def __str__(self):
        return f"Reg: {self.registro} - {self.fecha} - Riego zona {self.zona} - {self.accion} "

class riegoResumen(models.Model):
    registro = models.BigAutoField(primary_key=True)
    fecha = models.DateTimeField(auto_now_add=True)

    riego = models.OneToOneField(
        riegoRegistro,
        on_delete=models.CASCADE,
        related_name="resumen"
    )

    litros_usados = models.FloatField()

    def _str_(self):
        return f"Reg: {self.registro} - {self.feha} - Resumen zona {self.riego.zona} - {self.litros_usados} L"