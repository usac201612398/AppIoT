from django.shortcuts import render
from .models import m1Sensoresdata, m2Sensoresdata, riegoRegistro, riegoResumen
# Create your views here.
import time
import paho.mqtt.client as mqtt
import uuid
import os
import numpy as np
from django.http import JsonResponse
import json
from django.utils.dateparse import parse_datetime
from django.db.models import Q, Avg, Max
from django.utils import timezone
from datetime import timedelta, datetime
from collections import defaultdict


MQTT_HOST = "10.111.112.4"
MQTT_PORT = 1883
MQTT_USER = "sdc-iot"       
MQTT_PASS = "nuevacontraseña"  

def publicar_mqtt(accion, topic):
    """
    Esta función publica un mensaje MQTT en el topic dado.
    """
    retorno = "Esperando"

    try:
        client = mqtt.Client(client_id=f"django-{uuid.uuid4()}")
        client.username_pw_set(MQTT_USER, MQTT_PASS)
        client.connect(MQTT_HOST, MQTT_PORT, 60)
        client.loop_start()
        time.sleep(1)
        result = client.publish(topic, accion.upper(), qos=0)
        result.wait_for_publish()

        if result.rc == mqtt.MQTT_ERR_SUCCESS:
            retorno = "Publicado con éxito"
        else:
            retorno = f"Error código {result.rc}"

        client.loop_stop()
        client.disconnect()
        
    except Exception as e:
        retorno = f"Error MQTT: {str(e)}"

    return retorno

def enviarinstruccion(request):
    """
    Vista o logica de negocio para:
    - Riego on/off/auto
    - Riego con tiempo y válvula (modal)
    - Tanque manual
    """
    if request.method == "POST":
        # Se intenta recibir JSON (para riego manual con tiempo/valvula)
        if request.content_type == "application/json":
            data = json.loads(request.body)
            accion = data.get("accion")
            dispositivo = data.get("dispositivo")
            zona = data.get("valvula")  # opcional
            tiempo = data.get("tiempo")    # opcional
        else:
            # Botones normales (on/off/auto)
            accion = request.POST.get("accion")
            dispositivo = request.POST.get("dispositivo")
            zona = None
            tiempo = None

        # Se prepara payload para MQTT
        payload = {"accion": accion}
        if zona:
            payload["zona"] = zona
        if tiempo:
            payload["tiempo"] = tiempo

        payload_json = json.dumps(payload)

        # Selección de topic
        if dispositivo == "riego":
            topic = "casa/tanque/1/riego/manual"
        elif dispositivo == "tanque":
            topic = "casa/tanque/1/llenado/manual"
        else:
            return JsonResponse({"status": "error", "msg": "Dispositivo desconocido"})

        retorno = publicar_mqtt(payload_json, topic)

        return JsonResponse({
            "status": "ok",
            "accion": payload,
            "dispositivo": dispositivo,
            "retorno": retorno
        })

    return JsonResponse({"status": "error", "msg": "Método no permitido"})

def plantadashboard(request):
    plantas = m1Sensoresdata.objects.values_list('planta_id', flat=True).distinct()
    
    return render(request, 'iotappweb/plantadashboard.html', {
        'plantas': plantas
    })

def planta_api(request):
    try:
        planta_id = request.GET.get('planta_id')
        desde = request.GET.get('desde')
        hasta = request.GET.get('hasta')
        limite = int(request.GET.get('limite', 50))

        # Traer todos los datos ordenados descendente
        data = m1Sensoresdata.objects.all().order_by('-timestamp')

        if planta_id:
            data = data.filter(planta_id=planta_id)

        if desde:
            dt_desde = parse_datetime(desde)
            if dt_desde:
                data = data.filter(timestamp__gte=dt_desde)

        if hasta:
            dt_hasta = parse_datetime(hasta)
            if dt_hasta:
                data = data.filter(timestamp__lte=dt_hasta)

        # Solo últimos N registros
        data = list(data[:limite][::-1])  # Se invierte para ascendente

        def round2(v):
            return round(v, 2) if v is not None else 0

        # Fechas y valores para la gráfica
        timestamps = [timezone.localtime(d.timestamp).strftime("%H:%M:%S") for d in data]
        temperatura = [round2(d.temperatura) for d in data]
        humedad_aire = [round2(d.humedad_aire) for d in data]
        humedad_suelo = [round2(d.humedad_suelo) for d in data]
        peso = [round2(d.peso) for d in data]

        # Últimos datos por planta
        latest_by_planta = {}
        all_plantas = m1Sensoresdata.objects.values_list('planta_id', flat=True).distinct()
        for planta in all_plantas:
            last_entry = m1Sensoresdata.objects.filter(planta_id=planta).order_by('-timestamp').first()
            if last_entry:
                latest_by_planta[planta] = {
                    "temperatura": round2(last_entry.temperatura),
                    "humedad_aire": round2(last_entry.humedad_aire),
                    "humedad_suelo": round2(last_entry.humedad_suelo),
                    "peso": round2(last_entry.peso),
                    "timestamp": timezone.localtime(last_entry.timestamp).strftime("%Y-%m-%d %H:%M:%S")
                }

        # Último dato general
        latest = {}
        if planta_id and planta_id in latest_by_planta:
            latest = latest_by_planta[planta_id]
        elif latest_by_planta:
            latest = list(latest_by_planta.values())[0]

        # Promedio del día en curso
        today = timezone.localtime(timezone.now()).date()
        avg_today_qs = m1Sensoresdata.objects.filter(timestamp__date=today)
        if planta_id:
            avg_today_qs = avg_today_qs.filter(planta_id=planta_id)

        avg_today = {
            "temperatura": round2(avg_today_qs.aggregate(Avg('temperatura'))['temperatura__avg']),
            "humedad_aire": round2(avg_today_qs.aggregate(Avg('humedad_aire'))['humedad_aire__avg']),
            "humedad_suelo": round2(avg_today_qs.aggregate(Avg('humedad_suelo'))['humedad_suelo__avg']),
            "peso": round2(avg_today_qs.aggregate(Avg('peso'))['peso__avg']),
        }

        response = {
            "timestamps": timestamps,
            "temperatura": temperatura,
            "humedad_aire": humedad_aire,
            "humedad_suelo": humedad_suelo,
            "peso": peso,
            "latest": latest,
            "avg_today": avg_today,
            "latest_by_planta": latest_by_planta,
        }

        return JsonResponse(response)

    except Exception as e:
        # Devuelve error en JSON en lugar de HTML
        return JsonResponse({"error": str(e)}, status=500)

def tanquedashboard(request):
    return render(request, "iotappweb/tanquedashboard.html")

def historial_riegos(request):
    
    hace_7_dias = timezone.now() - timedelta(days=7)

    riegos = riegoRegistro.objects.filter(
        fecha__gte=hace_7_dias
    ).select_related('planta_reg', 'tanque_reg').order_by('-registro')

    return render(request, "iotappweb/historialderiegos.html", {
        "riegos": riegos
    })

def tanque_api(request):
    tanque_id = request.GET.get('tanque_id')
    desde = request.GET.get('desde')
    hasta = request.GET.get('hasta')
    limite = int(request.GET.get('limite', 50))

    data = m2Sensoresdata.objects.all().order_by('-timestamp')

    if tanque_id:
        data = data.filter(tanque_id=tanque_id)
    if desde:
        data = data.filter(timestamp__gte=parse_datetime(desde))
    if hasta:
        data = data.filter(timestamp__lte=parse_datetime(hasta))

    data = list(data[:limite])[::-1]

    def round2(v):
        return round(v, 2) if v is not None else 0

    response = {
        "timestamps": [
            timezone.localtime(d.timestamp).strftime("%H:%M:%S")
            for d in data
        ],
        "temperatura": [round2(d.temperatura) for d in data],
        "caudal": [round2(d.caudal) for d in data],
        "nivel": [round2(d.nivel) for d in data],
        "porcentaje_llenado": [round2(d.porcentaje_llenado) for d in data],
        "latest": {
            "temperatura": round2(data[-1].temperatura) if data else 0,
            "caudal": round2(data[-1].caudal) if data else 0,
            "nivel": round2(data[-1].nivel) if data else 0,
            
            "porcentaje_llenado": round2(data[-1].porcentaje_llenado) if data else 0,
            "timestamp": timezone.localtime(data[-1].timestamp).strftime("%Y-%m-%d %H:%M:%S") if data else ""
        }
    }
    return JsonResponse(response)

def consumo_acumulado(request):
    """
    Consumo acumulado por día para la semana actual, separado por válvula.
    """
    hoy = datetime.now()
    inicio_semana = hoy - timedelta(days=hoy.weekday())  # lunes
    fin_semana = inicio_semana + timedelta(days=7)       # domingo

    resumenes = riegoResumen.objects.filter(
        fecha__gte=inicio_semana,
        fecha__lt=fin_semana
    )

    # Para almacenar el consumo acumulado separado por válvula y fecha
    tabla = defaultdict(lambda: defaultdict(float))

    for resumen in resumenes:
        litros = resumen.litros_usados
        fecha = resumen.fecha.date()
        zona = resumen.riego.zona  
        valvula = 1 if zona == 1 else 2  
        
        # Se suman los litros consumidos por válvula y fecha
        tabla[fecha][valvula] += litros

    # Preparacion de datos para mostrarlos en el template
    fechas_litros = []
    for fecha in sorted(tabla.keys()):
        fila = {"fecha": fecha}
        fila["valvula_1"] = tabla[fecha].get(1, 0)
        fila["valvula_2"] = tabla[fecha].get(2, 0)
        fechas_litros.append(fila)

    context = {
        "fechas_litros": fechas_litros,
    }

    return render(request, "consumo_acumulado_modal.html", context)

def histograma_api(request):
    
    planta = request.GET.get("planta_id")
    variable = request.GET.get("variable")
    desde = request.GET.get("desde")
    hasta = request.GET.get("hasta")

    if variable not in ["temperatura", "humedad_aire", "humedad_suelo", "peso"]:
        return JsonResponse({"error": "Variable inválida"}, status=400)

    queryset = m1Sensoresdata.objects.all()
    
    if planta:
        queryset = queryset.filter(planta_id=planta)
    if desde:
        queryset = queryset.filter(timestamp__gte=desde)
    if hasta:
        queryset = queryset.filter(timestamp__lte=hasta)

    valores = np.array(list(queryset.values_list(variable, flat=True)), dtype=float)

    if valores.size == 0:
        return JsonResponse({
            "bins": [],
            "counts": [],
            "media": 0,
            "mediana": 0,
            "std": 0,
            "min": 0,
            "max": 0,
            "total_mediciones": 0
        })

    counts, bins = np.histogram(valores, bins=10)
    media = float(np.mean(valores))
    mediana = float(np.median(valores))
    std = float(np.std(valores))
    min_val = float(np.min(valores))
    max_val = float(np.max(valores))
    total_mediciones = valores.size

    return JsonResponse({
        "bins": [round(b,2) for b in bins[:-1]],
        "counts": counts.tolist(),
        "media": round(media,2),
        "mediana": round(mediana,2),
        "std": round(std,2),
        "min": round(min_val,2),
        "max": round(max_val,2),
        "total_mediciones": total_mediciones
    })