with open('transparency-web/public/js/auth.js', 'r') as f:
    content = f.read()

signup_start = content.find('signupForm.addEventListener("submit"')
signup_end = content.find('});\n\n// Login') + 3

print("--- SIGNUP ---")
print(content[signup_start:signup_end])
